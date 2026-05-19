#!/usr/bin/env python3
"""
swo_profiler.py — PC-sampling profiler for Cortex-M via SWO/DWT + J-Link
Supported targets:
  - MIMXRT1170-EVK  (Cortex-M7 @ 996 MHz) -- uses NXP proprietary SWO block
  - MIMXRT1060-EVK  (Cortex-M7 @ 600 MHz) -- uses ARM CoreSight standard TPIU

Requires:
    pip install pylink-square
    SEGGER J-Link Software installed (JLinkARM.dll / libjlinkarm.so)
    arm-none-eabi-addr2line in PATH

Usage:
    # RT1170 (996 MHz core, 132 MHz trace clock, prescaler 99 → 1.32 Mbaud SWO)
    python swo_profiler.py --device MIMXRT1176xxx8_M7 --elf firmware.elf

    # RT1170 with prescaler 9 → 13.2 Mbaud (short cables only)
    python swo_profiler.py --device MIMXRT1176xxx8_M7 --elf firmware.elf --prescaler 9

    # RT1060 (600 MHz core, standard TPIU)
    python swo_profiler.py --device MIMXRT1062xxx6A --elf firmware.elf

Key options:
    --trace-clk   Trace peripheral clock Hz (RT1170: 132000000, RT1060: = cpu-freq)
    --prescaler   Divisor written to SWO_CODR/TPIU_ACPR: baud = trace_clk/(N+1)
                  RT1170 default: 99 (→1.32 Mbaud). Overrides --swo-freq.
    --cpu-freq    Core clock Hz (only for DWT sample-rate display)
    --swo-freq    Desired baud (ignored if --prescaler given; used for RT1060)
    --duration    Sampling duration seconds (default: 10)
    --post-reset  DWT POSTPRESET 1..15 (default: 15, lower = more samples/sec)
    --post-tap    DWT CYCTAP: 0=bit6 (/64), 1=bit10 (/1024) (default: 1)
    --top-n       Hotspot entries to show (default: 40)
    --by-line     Aggregate by source line instead of function
    --output      Save raw PC list to file
    --addr2line   Path to addr2line binary
"""

import argparse
import collections
import struct
import subprocess
import sys
import time

try:
    import pylink
except ImportError:
    print("ERROR: pylink-square not installed. Run: pip install pylink-square")
    sys.exit(1)


# ---------------------------------------------------------------------------
# Target register maps
# ---------------------------------------------------------------------------

class _RegMapRT1170:
    """
    MIMXRT1170: NXP proprietary SWO block at 0xE0048000.
    The standard ARM TPIU at 0xE0040000 is NOT present on this chip —
    writing there causes a bus fault.

    CRITICAL: The SWO baud rate is driven by the TRACE clock domain
    (CCM_CLKROOT_TRACE, default 132 MHz), NOT the Cortex-M7 core clock.

    SWO_CODR = (trace_clk / desired_baud) - 1
    Examples at 132 MHz trace clock:
      prescaler=99  → 1.320 Mbaud   (safe, recommended default)
      prescaler=28  → 4.552 Mbaud   (NXP SDK example value 0x1C)
      prescaler=9   → 13.20 Mbaud   (short cable only)
    """
    NAME              = "RT1170"
    DEFAULT_CPU_FREQ  = 996_000_000
    DEFAULT_TRACE_CLK = 132_000_000
    DEFAULT_PRESCALER = 99            # → 1.32 Mbaud

    DEMCR      = 0xE000EDFC
    DWT_CTRL   = 0xE0001000
    DWT_CYCCNT = 0xE0001004
    ITM_LAR    = 0xE0000FB0
    ITM_TCR    = 0xE0000E80
    ITM_TER    = 0xE0000E00

    SWO_LAR    = 0xE0048FB0   # Lock Access (write LOCK_KEY)
    SWO_CODR   = 0xE0048010   # Current Output Divisor: baud = trace_clk/(CODR+1)
    SWO_SPPR   = 0xE00480F0   # Pin Protocol: 2=NRZ/UART

    CSTF1_LAR  = 0xE0045FB0
    CSTF1_CTRL = 0xE0045000   # 0x3FF = enable all input ports
    CSTF2_LAR  = 0xE0043FB0
    CSTF2_CTRL = 0xE0043000

    LOCK_KEY   = 0xC5ACCE55


class _RegMapRT1060:
    """
    MIMXRT1060: standard ARM CoreSight TPIU at 0xE0040000.
    Trace clock = core clock (no separate domain).
    TPIU_ACPR = (cpu_freq / desired_baud) - 1
    """
    NAME              = "RT1060"
    DEFAULT_CPU_FREQ  = 600_000_000
    DEFAULT_TRACE_CLK = 600_000_000
    DEFAULT_PRESCALER = None          # computed from --swo-freq

    DEMCR      = 0xE000EDFC
    DWT_CTRL   = 0xE0001000
    DWT_CYCCNT = 0xE0001004
    ITM_LAR    = 0xE0000FB0
    ITM_TCR    = 0xE0000E80
    ITM_TER    = 0xE0000E00

    TPIU_LAR   = 0xE0040FB0
    TPIU_ACPR  = 0xE0040010
    TPIU_SPPR  = 0xE00400F0
    TPIU_FFCR  = 0xE0040304

    LOCK_KEY   = 0xC5ACCE55


def _detect_regmap(device: str):
    dev = device.upper()
    if any(x in dev for x in ("RT117", "RT1176", "RT1175", "RT1173")):
        return _RegMapRT1170
    if any(x in dev for x in ("RT106", "RT1062", "RT1064", "RT1061")):
        return _RegMapRT1060
    print(f"  [WARN] Unknown device '{device}', assuming RT1060 register map.")
    return _RegMapRT1060


# ---------------------------------------------------------------------------
# Register helpers
# ---------------------------------------------------------------------------

def _wr(jlink, addr, value, label=""):
    jlink.memory_write32(addr, [value])
    print(f"    WR  0x{addr:08X} = 0x{value:08X}" + (f"  [{label}]" if label else ""))


def _rd(jlink, addr, label=""):
    val = jlink.memory_read32(addr, 1)[0]
    print(f"    RD  0x{addr:08X} = 0x{val:08X}" + (f"  [{label}]" if label else ""))
    return val


# ---------------------------------------------------------------------------
# Trace setup / teardown
# ---------------------------------------------------------------------------

def setup_trace(jlink, regmap, trace_clk, prescaler, post_tap, post_reset):
    """
    Configure DWT PC-sampling and SWO output.
    Call with core HALTED.
    """
    R = regmap
    actual_baud = trace_clk / (prescaler + 1)
    print(f"  Target    : {R.NAME}")
    print(f"  TraceCLK  : {trace_clk/1e6:.3f} MHz")
    print(f"  Prescaler : {prescaler}  →  {actual_baud/1e6:.4f} Mbaud")

    # Step 1: disable DWT/PC sampling before reconfiguring
    print("  [1] Disable DWT (safe state)")
    _wr(jlink, R.DWT_CTRL, 0x400002EF, "DWT_CTRL safe-1")
    _wr(jlink, R.DWT_CTRL, 0x400002EE, "DWT_CTRL safe-2")

    # Step 2: enable CoreSight trace blocks (TRCENA in DEMCR)
    print("  [2] Enable TRCENA")
    _wr(jlink, R.DEMCR, 0x01000000, "DEMCR TRCENA")

    # Step 3: configure SWO output peripheral
    print("  [3] Configure SWO output")
    if R is _RegMapRT1170:
        _wr(jlink, R.SWO_LAR,    R.LOCK_KEY, "SWO_LAR unlock")
        _wr(jlink, R.CSTF1_LAR,  R.LOCK_KEY, "CSTF1_LAR unlock")
        _wr(jlink, R.CSTF2_LAR,  R.LOCK_KEY, "CSTF2_LAR unlock")
        _wr(jlink, R.CSTF1_CTRL, 0x3FF,      "CSTF1 enable ports")
        _wr(jlink, R.CSTF2_CTRL, 0x3FF,      "CSTF2 enable ports")
        _wr(jlink, R.SWO_SPPR,   0x2,        "SWO_SPPR NRZ")
        _wr(jlink, R.SWO_CODR,   prescaler,  "SWO_CODR")
    else:
        _wr(jlink, R.TPIU_LAR,  R.LOCK_KEY, "TPIU_LAR unlock")
        _wr(jlink, R.TPIU_ACPR, prescaler,  "TPIU_ACPR")
        _wr(jlink, R.TPIU_SPPR, 0x2,        "TPIU_SPPR NRZ")
        _wr(jlink, R.TPIU_FFCR, 0x0,        "TPIU_FFCR bypass")

    # Step 4: configure ITM (disable → enable DWT path → enable ITM)
    print("  [4] Configure ITM")
    _wr(jlink, R.ITM_LAR, R.LOCK_KEY, "ITM_LAR unlock")
    _wr(jlink, R.ITM_TCR, 0x0001000C, "ITM_TCR disable")
    # ITM_TCR final value: ITMENA=1, TSENA=0, TXENA=1, TraceBusID=1
    # TSENA=0: disable local timestamps (they add 1-5 bytes per DWT packet
    #          and stall the ITM pipeline — primary cause of overflows)
    # GTSFREQ=00: disable global timestamps (saves more bandwidth)
    # The original value 0x0001040B had TSENA=1+GTSFREQ=01 which caused
    # ~40% extra ITM traffic and filled the FIFO between DWT packets.
    _wr(jlink, R.ITM_TCR, 0x00010009, "ITM_TCR enable (no timestamps)")
    _wr(jlink, R.ITM_TER, 0x00000001, "ITM_TER port 0")

    # Step 5: enable DWT cycle counter + PC sampling
    # DWT_CTRL bits: [0]=CYCCNTENA, [9]=CYCTAP, [13:10]=POSTPRESET,
    #                [17:14]=POSTINIT, [22]=PCSAMPLEENA
    print("  [5] Enable DWT PC sampling")
    p4 = post_reset & 0xF
    # DWT_CTRL bits set:
    #   [0]      CYCCNTENA   = 1  (cycle counter on)
    #   [9]      CYCTAP      = post_tap
    #   [13:10]  POSTPRESET  = p4
    #   [17:14]  POSTINIT    = p4
    #   [22]     PCSAMPLEENA = 1
    # Bits explicitly left 0:
    #   [16]     EXCTRCENA   = 0  (exception trace OFF — each IRQ entry/exit
    #                               emits a 3-byte packet, floods ITM with
    #                               unneeded traffic and causes overflows)
    #   [19]     FOLDEVTENA  = 0  (folded instruction counter, not needed)
    #   [20]     LSUEVTENA   = 0  (LSU counter, not needed)
    #   [21]     SLEEPEVTENA = 0  (sleep counter, not needed)
    dwt_ctrl = (1 << 0) | (post_tap << 9) | (p4 << 10) | (p4 << 14) | (1 << 22)
    _wr(jlink, R.DWT_CYCCNT, 0x00000000, "DWT_CYCCNT clear")
    _wr(jlink, R.DWT_CTRL,   dwt_ctrl,   "DWT_CTRL enable (EXCTRCENA=0)")

    divisor = (1 << (6 if post_tap == 0 else 10)) * (p4 + 1)
    cpu = regmap.DEFAULT_CPU_FREQ
    print(f"  DWT: post_tap={post_tap} post_reset={p4} "
          f"→ ~{cpu/divisor:.0f} PC samples/sec (at {cpu/1e6:.0f} MHz)")


def stop_trace(jlink, regmap):
    """Disable DWT/ITM. Call with core HALTED."""
    R = regmap
    print("  Disabling DWT PC sampling...")
    _wr(jlink, R.DWT_CTRL, 0x400002EE, "DWT_CTRL PC sample off")
    print("  Disabling ITM...")
    _wr(jlink, R.ITM_LAR,  R.LOCK_KEY, "ITM_LAR unlock")
    _wr(jlink, R.ITM_TCR,  0x0001000C, "ITM_TCR disable ITM")
    print("  Clearing TRCENA...")
    _wr(jlink, R.DEMCR, 0x00000000, "DEMCR TRCENA off")


# ---------------------------------------------------------------------------
# Register readback diagnostics
# ---------------------------------------------------------------------------

def verify_registers(jlink, regmap, trace_clk, prescaler):
    R = regmap
    print("  [verify] Reading back registers...")

    def chk(addr, name, expected=None, mask=None):
        try:
            val = jlink.memory_read32(addr, 1)[0]
            chkval = (val & mask) if mask else val
            if expected is not None:
                status = "OK" if chkval == expected else f"*** MISMATCH expected=0x{expected:08X}"
            else:
                status = ""
            print(f"    RD  0x{addr:08X} = 0x{val:08X}  [{name}]  {status}")
            return val
        except Exception as e:
            print(f"    RD  0x{addr:08X} ERROR [{name}]: {e}")
            return None

    demcr = chk(R.DEMCR,    "DEMCR",    expected=0x01000000, mask=0x01000000)
    # DWT_CTRL: only verify the bits we explicitly set.
    # The chip auto-manages POSTCNT [9:5], CYCCNT state, and other status bits,
    # so a full-word compare would always mismatch.
    # PCSAMPLEENA = bit22, CYCCNTENA = bit0
    dwt   = chk(R.DWT_CTRL, "DWT_CTRL")
    tcr   = chk(R.ITM_TCR,  "ITM_TCR",  expected=0x00010009, mask=0x0001000F)
    chk(R.ITM_TER, "ITM_TER", expected=0x1)

    if demcr is not None and not (demcr & (1<<24)):
        print("    *** WARN: TRCENA=0 → ALL trace blocks are off!")
    if dwt is not None:
        if not (dwt & 1):
            print("    *** WARN: CYCCNTENA=0 → cycle counter off!")
        if not (dwt & (1<<22)):
            # NOTE: on RT1170 PCSAMPLEENA may read back as 0 even when active
            # (write-only behaviour on some silicon revisions). If samples are
            # arriving this warning is a false positive.
            print("    [info] PCSAMPLEENA (DWT_CTRL bit22) reads 0 — may be write-only "
                  "on this silicon. If PC samples arrive, ignore this.")

    if R is _RegMapRT1170:
        codr = chk(R.SWO_CODR,   "SWO_CODR",   expected=prescaler)
        sppr = chk(R.SWO_SPPR,   "SWO_SPPR",   expected=2)
        # CSTF on RT1170 only exposes 2 input ports (0 and 1).
        # EnS (HoldEnable) bit8 and PortEnable bits [1:0] → 0x303 is correct.
        # Writing 0x3FF sets all bits; the chip masks to supported ones.
        cstf1 = chk(R.CSTF1_CTRL, "CSTF1_CTRL")
        if cstf1 is not None and (cstf1 & 0x3) == 0:
            print("    *** WARN: CSTF1 port enable bits are 0 — DWT data won't reach SWO!")
        if codr is not None:
            print(f"    INFO: actual baud = {trace_clk/(codr+1)/1e6:.4f} Mbaud")
        if sppr is not None and sppr != 2:
            print(f"    *** WARN: SWO_SPPR={sppr:#x} expected 0x2 (NRZ)!")
    else:
        acpr = chk(R.TPIU_ACPR, "TPIU_ACPR", expected=prescaler)
        chk(R.TPIU_SPPR, "TPIU_SPPR", expected=2)
        chk(R.TPIU_FFCR, "TPIU_FFCR", expected=0)
        if acpr is not None:
            print(f"    INFO: actual baud = {trace_clk/(acpr+1)/1e6:.4f} Mbaud")


# ---------------------------------------------------------------------------
# ITM/DWT stream decoder
# ---------------------------------------------------------------------------

class ITMDecoder:
    """
    Complete ARM CoreSight ITM/DWT stream decoder (ARM DDI0403E Table D4-3).
    Tracks a full packet-type histogram for diagnostics.
    """
    def __init__(self, on_pc_sample, on_overflow=None, on_sleep=None):
        self.on_pc_sample   = on_pc_sample
        self.on_overflow    = on_overflow or (lambda: None)
        self.on_sleep       = on_sleep    or (lambda: None)
        self._buf           = bytearray()
        self._sync_zeros    = 0
        self.total_bytes    = 0
        self.total_samples  = 0
        self.total_overflow = 0
        self.total_sleep    = 0
        self.total_corrupt  = 0
        self.total_unknown  = 0
        self.total_exctrace = 0
        self.pkt_counts     = collections.Counter()  # {type_str: packet count}
        self.pkt_bytes      = collections.Counter()  # {type_str: byte count}

    def feed(self, data: bytes):
        self._buf.extend(data)
        self._parse()

    def _consume(self, ptype, nbytes):
        self.pkt_counts[ptype] += 1
        self.pkt_bytes[ptype]  += nbytes

    def _parse(self):
        """
        Packet types handled per ARM DDI0403E D4.3:
          Sync            5x 0x00 + 0x80
          Overflow        0x70 (1B)
          Local TS        header bits[3:0]=0000, variable length
          Global TS GTS1  0x94 + continuation
          Global TS GTS2  0xB4 + continuation
          DWT PC sleep    0x15 (1B, no payload)
          DWT PC sample   0x17 + 4B payload
          DWT exc trace   0x0E + 2B payload
          DWT event ctr   0x05 + 1B payload
          HW source       discriminator in bits[7:3], size in bits[1:0]
          ITM stimulus    port in bits[7:3], size in bits[1:0]
        """
        buf = self._buf
        i, n = 0, len(buf)

        while i < n:
            b = buf[i]

            # ---- Synchronization ----
            if b == 0x00:
                self._sync_zeros += 1
                self._consume("sync_zero", 1)
                i += 1; continue

            if b == 0x80 and self._sync_zeros >= 5:
                self._sync_zeros = 0
                self._consume("sync_end", 1)
                i += 1; continue

            self._sync_zeros = 0

            # ---- Overflow (1B) ----
            if b == 0x70:
                self._consume("overflow", 1)
                self.total_overflow += 1
                self.on_overflow()
                i += 1; continue

            # ---- Local timestamp ----
            # bits[3:0]=0000 with non-zero byte (excluding 0x00, 0x70, 0x80)
            # bit7=C: 1=continuation byte follows, 0=final byte
            if (b & 0x0F) == 0x00 and b not in (0x00, 0x70, 0x80):
                # Local timestamp. bit7 of header = C (continuation):
                #   bit7=0: this byte IS the complete packet
                #   bit7=1: continuation bytes follow (each with bit7=1), terminated
                #           by a final byte with bit7=0
                start_i = i; i += 1
                if b & 0x80:          # C=1: continuation bytes present
                    while i < n and (buf[i] & 0x80): i += 1
                    if i < n: i += 1  # consume final byte (bit7=0)
                # C=0: header is the complete packet, i already advanced past it
                nb = i - start_i
                self._consume(f"local_ts({nb}B)", nb)
                continue

            # 0x80 without sync context = local timestamp: TC=0, C=1 → has continuation
            if b == 0x80:
                start_i = i; i += 1
                while i < n and (buf[i] & 0x80): i += 1
                if i < n: i += 1
                nb = i - start_i
                self._consume(f"local_ts_0x80({nb}B)", nb)
                continue

            # ---- Global timestamps ----
            if b in (0x94, 0xB4):
                label = "GTS1" if b == 0x94 else "GTS2"
                start_i = i; i += 1
                while i < n and (buf[i] & 0x80): i += 1
                if i < n: i += 1
                nb = i - start_i
                self._consume(f"{label}({nb}B)", nb)
                continue

            # ---- DWT PC sample (sleep) — header only, no payload ----
            if b == 0x15:
                self._consume("DWT_PC_sleep(1B)", 1)
                self.total_sleep += 1
                self.on_sleep()
                i += 1; continue

            # ---- DWT PC sample (running) — 4B payload ----
            if b == 0x17:
                if i + 4 >= n: break
                pc = struct.unpack_from('<I', buf, i+1)[0]
                if pc & 1:
                    self._consume("corrupt_PC(5B)", 5)
                    self.total_corrupt += 1
                else:
                    self._consume("DWT_PC_sample(5B)", 5)
                    self.total_samples += 1
                    self.on_pc_sample(pc)
                i += 5; continue

            # ---- DWT exception trace — 2B payload ----
            if b == 0x0E:
                if i + 2 >= n: break
                self._consume("DWT_exc_trace(3B)", 3)
                self.total_exctrace += 1
                i += 3; continue

            # ---- DWT event counter wrapper — 1B payload ----
            if b == 0x05:
                if i + 1 >= n: break
                self._consume("DWT_event_ctr(2B)", 2)
                i += 2; continue

            # ---- Generic sized packets (HW source + ITM stimulus) ----
            # bits[1:0] encode payload size: 01=1B, 10=2B, 11=4B
            size_enc = b & 0x03
            if size_enc != 0:
                psz = 1 << (size_enc - 1)
                if i + psz >= n: break
                disc = (b >> 3) & 0x1F
                port = (b >> 3) & 0x1F
                # Discriminate HW (disc ID in known DWT range) vs ITM SW stimulus
                # ITM stimulus: any port (0-31), size 1/2/4B, lo bits = size_enc
                # DWT HW:       disc IDs 0-23 per ARM spec, lo bits = size_enc
                # In practice both use the same format; label by disc value
                label = f"HW_disc{disc}({1+psz}B)"
                self._consume(label, 1 + psz)
                i += 1 + psz; continue

            # ---- Unknown ----
            self._consume(f"unknown_0x{b:02X}(1B)", 1)
            self.total_unknown += 1
            i += 1

        self._buf = buf[i:]
        self.total_bytes += i

class SymbolResolver:
    """
    Resolve PC addresses to (function, file:line) via addr2line.

    ITCM alias remapping (RT1170 / RT1060):
      Code linked at 0x00200000 (ITCM physical) runs at 0x00000000 (ITCM alias).
      DWT emits the runtime PC (0x000xxxxx), but the ELF has symbols at 0x002xxxxx.
      We try the raw address first; if unresolved we retry with a configurable offset.

    addr_remaps: list of (match_mask, match_value, add_offset)
      If (pc & match_mask) == match_value, retry with pc + add_offset.
    Default covers RT1170 ITCM alias: 0x000xxxxx → +0x00200000 → 0x002xxxxx
    """

    # (mask, value, offset_to_add)
    DEFAULT_REMAPS = [
        (0xFFF00000, 0x00000000, 0x00200000),  # ITCM alias 0x000xxxxx → 0x002xxxxx
    ]

    def __init__(self, elf_path, addr2line="arm-none-eabi-addr2line",
                 addr_remaps=None):
        self.elf_path    = elf_path
        self.addr2line   = addr2line
        self.addr_remaps = addr_remaps if addr_remaps is not None else self.DEFAULT_REMAPS
        self._cache      = {}
        try:
            r = subprocess.run([addr2line, "--version"], capture_output=True, timeout=5)
            if r.returncode != 0: raise RuntimeError
        except FileNotFoundError:
            print(f"ERROR: '{addr2line}' not found. Install arm-none-eabi binutils.")
            sys.exit(1)

    def _remap(self, addr):
        """Return remapped address if a remap rule matches, else None."""
        for mask, value, offset in self.addr_remaps:
            if (addr & mask) == value:
                return (addr + offset) & 0xFFFFFFFF
        return None

    def _batch_query(self, addresses):
        """Call addr2line for a list of addresses, return {addr: (fn, loc)}."""
        if not addresses:
            return {}
        r = subprocess.run(
            [self.addr2line, "-e", self.elf_path, "-f", "-C", "-p"]
            + [f"0x{a:08x}" for a in addresses],
            capture_output=True, text=True, timeout=30)
        result = {}
        for addr, line in zip(addresses, r.stdout.strip().split("\n")):
            if " at " in line:
                fn, loc = line.split(" at ", 1)
                result[addr] = (fn.strip(), loc.strip())
            else:
                result[addr] = ("??", "??:0")
        return result

    def resolve_many(self, addresses):
        uncached = [a for a in addresses if a not in self._cache]

        # First pass: resolve all uncached addresses directly
        for i in range(0, len(uncached), 500):
            self._cache.update(self._batch_query(uncached[i:i+500]))

        # Second pass: retry unresolved addresses with remapping
        unresolved = [a for a in uncached if self._cache.get(a, ("??",))[0] == "??"]
        if unresolved and self.addr_remaps:
            remap_pairs = {}   # remapped_addr → original_addr
            for a in unresolved:
                ra = self._remap(a)
                if ra is not None and ra != a:
                    remap_pairs[ra] = a

            if remap_pairs:
                remap_addrs = list(remap_pairs.keys())
                for i in range(0, len(remap_addrs), 500):
                    batch_result = self._batch_query(remap_addrs[i:i+500])
                    for ra, res in batch_result.items():
                        orig = remap_pairs[ra]
                        if res[0] != "??":
                            # Found via remap — tag the function name so it's visible
                            fn, loc = res
                            self._cache[orig] = (fn, loc)
                            # (optionally could append " [ITCM]" to fn for clarity)

        return {a: self._cache[a] for a in addresses}


# ---------------------------------------------------------------------------
# PC address distribution diagnostics
# ---------------------------------------------------------------------------

def _print_pc_distribution(pc_samples):
    """
    Print a breakdown of PC samples by address region.
    Helps identify if ITCM (0x000xxxxx) or other regions are present but unresolved.
    """
    regions = {
        "ITCM  0x00000000-0x000FFFFF": (0x00000000, 0x000FFFFF),
        "ITCM  0x00100000-0x001FFFFF": (0x00100000, 0x001FFFFF),
        "ITCM  0x00200000-0x002FFFFF": (0x00200000, 0x002FFFFF),  # ELF link addr
        "DTCM  0x20000000-0x2001FFFF": (0x20000000, 0x2001FFFF),
        "OCRAM 0x20200000-0x2037FFFF": (0x20200000, 0x2037FFFF),
        "Flash 0x30000000-0x3FFFFFFF": (0x30000000, 0x3FFFFFFF),
        "Flash 0x60000000-0x6FFFFFFF": (0x60000000, 0x6FFFFFFF),
        "Other": None,
    }
    total = len(pc_samples)
    if total == 0:
        return
    counts = collections.Counter(pc_samples)
    region_counts = collections.defaultdict(int)
    for pc, cnt in counts.items():
        placed = False
        for name, rng in regions.items():
            if rng is None:
                continue
            if rng[0] <= pc <= rng[1]:
                region_counts[name] += cnt
                placed = True
                break
        if not placed:
            region_counts["Other"] += cnt

    print("\nPC address region breakdown:")
    for name, rng in regions.items():
        cnt = region_counts.get(name, 0)
        if cnt > 0:
            pct = 100.0 * cnt / total
            print(f"  {pct:6.2f}%  {cnt:8d}  {name}")
    print()


# ---------------------------------------------------------------------------
# Main profiler
# ---------------------------------------------------------------------------

def run_profiler(args):
    regmap = _detect_regmap(args.device)

    trace_clk = args.trace_clk or regmap.DEFAULT_TRACE_CLK
    cpu_freq  = args.cpu_freq  or regmap.DEFAULT_CPU_FREQ

    if args.prescaler is not None:
        prescaler = args.prescaler
    elif regmap.DEFAULT_PRESCALER is not None:
        prescaler = regmap.DEFAULT_PRESCALER
    else:
        prescaler = (trace_clk // args.swo_freq) - 1

    actual_baud = trace_clk / (prescaler + 1)

    print(f"Target       : {regmap.NAME}")
    print(f"Core clock   : {cpu_freq/1e6:.3f} MHz")
    print(f"Trace clock  : {trace_clk/1e6:.3f} MHz")
    print(f"Prescaler    : {prescaler}  →  {actual_baud/1e6:.4f} Mbaud")

    pc_samples     = []
    sleep_count    = [0]
    overflow_count = [0]
    raw_bytes_rx   = [0]

    def on_overflow():
        overflow_count[0] += 1
        if overflow_count[0] == 1:
            print("  [!] ITM overflow detected — DWT is generating samples faster than "
                  "ITM can emit them.", flush=True)
            print("      Reduce sample rate: increase --post-reset (max 15) "
                  "or use --post-tap 1", flush=True)

    decoder = ITMDecoder(
        on_pc_sample = pc_samples.append,
        on_overflow  = on_overflow,
        on_sleep     = lambda: sleep_count.__setitem__(0, sleep_count[0]+1),
    )

    # Connect
    print(f"\nConnecting to {args.device}...")
    jlink = pylink.JLink()
    jlink.open()
    jlink.set_tif(pylink.enums.JLinkInterfaces.SWD)
    jlink.connect(args.device, verbose=False)
    print(f"  Core ID: 0x{jlink.core_id():08X}")

    # Halt before touching trace registers
    print("  Halting core...")
    jlink.halt()
    time.sleep(0.05)

    # Configure trace registers on chip
    print("\nSetting up trace registers...")
    setup_trace(jlink, regmap,
                trace_clk  = trace_clk,
                prescaler  = prescaler,
                post_tap   = args.post_tap,
                post_reset = args.post_reset)

    # Verify writes landed
    print()
    verify_registers(jlink, regmap, trace_clk, prescaler)

    # Start J-Link host-side SWO capture at the actual baud rate.
    # swo_start() only initialises the USB receiver — it does NOT write
    # any target registers, so it is safe for RT1170.
    print(f"\nStarting J-Link SWO capture @ {actual_baud/1e6:.4f} Mbaud...")
    try:
        jlink.swo_start(swo_speed=int(actual_baud))
    except TypeError:
        if regmap is _RegMapRT1170:
            print("ERROR: pylink swo_start() needs a speed argument.")
            print("       Upgrade: pip install -U pylink-square")
            jlink.close()
            sys.exit(1)
        # RT1060 fallback: swo_enable() is safe because TPIU is present
        jlink.swo_enable(cpu_speed=cpu_freq,
                         swo_speed=int(actual_baud),
                         port_mask=0x01)

    jlink.swo_flush()  # discard any stale buffered bytes

    # Resume core
    print("  Resuming core...")
    jlink.restart()
    time.sleep(0.05)

    # Sample loop
    print(f"\nSampling for {args.duration}s...  (Ctrl+C to stop early)\n")
    start = time.time()
    last_report = start
    try:
        while time.time() - start < args.duration:
            n = jlink.swo_num_bytes()
            if n > 0:
                raw = jlink.swo_read(offset=0, num_bytes=n, remove=True)
                raw_bytes_rx[0] += len(raw)
                decoder.feed(bytes(raw))

            now = time.time()
            if now - last_report >= 1.0:
                el        = now - start
                bw        = raw_bytes_rx[0] / el if el > 0 else 0
                n_samp    = len(pc_samples)
                n_ov      = overflow_count[0]
                # "decoded" bytes = bytes we have a packet type for
                decoded_b = sum(decoder.pkt_bytes.values())
                quality   = 100.0 * decoded_b / raw_bytes_rx[0] if raw_bytes_rx[0] else 0.0
                print(f"  t={el:5.1f}s  "
                      f"bw={bw/1e3:6.1f}kB/s  "
                      f"PC={n_samp:7d}  "
                      f"overflow={n_ov:7d}  "
                      f"decoded={quality:5.1f}%",
                      flush=True)
                last_report = now

            time.sleep(0.005)

    except KeyboardInterrupt:
        print("\nStopped by user.")

    elapsed = time.time() - start

    # Stop trace: halt → disable → resume
    print("\nStopping trace (halt → disable registers → resume)...")
    jlink.halt()
    time.sleep(0.02)
    stop_trace(jlink, regmap)
    jlink.swo_stop()
    jlink.restart()

    print(f"\nTotal: {len(pc_samples)} PC samples in {elapsed:.1f}s  "
          f"({len(pc_samples)/elapsed:.0f}/s)  "
          f"raw bytes rx: {raw_bytes_rx[0]}")
    rb  = raw_bytes_rx[0]
    b_decoded = sum(decoder.pkt_bytes.values())
    b_missing = rb - b_decoded
    b_pc = decoder.total_samples * 5
    b_ov = decoder.total_overflow * 1
    pct_loss = 100.0 * b_ov / (decoder.total_samples + decoder.total_overflow) \
               if (decoder.total_samples + decoder.total_overflow) else 0.0

    print()
    print("=== Packet accounting ===")
    print(f"  Raw bytes received  : {rb:>12,}  ({rb/elapsed/1e3:.1f} kB/s avg)")
    print(f"  Decoded by parser   : {b_decoded:>12,}  ({100*b_decoded/rb:.1f}%)")
    print(f"  Truly unaccounted   : {b_missing:>12,}  ({100*b_missing/rb:.1f}%)")
    print()
    print("  Packet type breakdown:")
    for ptype, pbytes in sorted(decoder.pkt_bytes.items(), key=lambda x: -x[1]):
        pct = 100.0 * pbytes / rb if rb else 0
        cnt = decoder.pkt_counts[ptype]
        if pbytes > 0:
            print(f"    {ptype:<32s}: {pbytes:>10,} B  {pct:5.1f}%  ({cnt:,} pkts)")
    print()
    print(f"  Sample loss rate    : {pct_loss:.1f}% of DWT events dropped (overflow)")
    print(f"  PC sample payload   : {100.0*b_pc/rb if rb else 0:.1f}% of raw bytes")

    ts_bytes = sum(v for k,v in decoder.pkt_bytes.items() if "ts" in k.lower() or "GTS" in k)
    if ts_bytes > rb * 0.03:
        print(f"  [!] {100.0*ts_bytes/rb:.1f}% timestamp traffic — firmware may be re-writing")
        print(f"      ITM_TCR at runtime (re-enabling TSENA/GTSFREQ after our setup).")
        print(f"      Halt core mid-run and read 0xE0000E80 to confirm.")
    if b_missing > rb * 0.02:
        print(f"  [!] {100*b_missing/rb:.1f}% bytes unaccounted — baud mismatch or signal issue.")
    if decoder.total_corrupt > 0:
        print(f"  [!] {decoder.total_corrupt} corrupt PC packets — check baud/signal quality.")

    if args.output:
        with open(args.output, "w") as f:
            for pc in pc_samples:
                f.write(f"0x{pc:08x}\n")
        print(f"Saved: {args.output}")

    if not pc_samples:
        print("\n*** No PC samples collected.")
        if raw_bytes_rx[0] == 0:
            print("    raw_bytes=0: J-Link received nothing on the SWO pin.")
            print("    → Is SWO pin connected? (separate from SWD CLK/DIO)")
            if regmap is _RegMapRT1170:
                print("    → RT1170 SWO pin: IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_00 = 0x00 (ALT0)")
            else:
                print("    → RT1060 SWO pin: IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_10 = ALT6")
            print("    → Try very slow baud to rule out timing: --prescaler 1319 (→ 100 kBaud)")
        else:
            print(f"    raw_bytes={raw_bytes_rx[0]}: bytes received but no 0x17 packets decoded.")
            print("    → Baud mismatch: J-Link baud ≠ chip baud.")
            print(f"      Chip baud = trace_clk/(prescaler+1) = {trace_clk}/{prescaler+1}"
                  f" = {actual_baud/1e6:.4f} Mbaud")
            print("    → Verify --trace-clk matches CCM_CLKROOT_TRACE on your board")
            print("    → Check register readback above for PCSAMPLEENA=0 or TRCENA=0")
        return

    # PC address distribution diagnostics
    _print_pc_distribution(pc_samples)

    # Symbol resolution
    print(f"\nResolving {len(set(pc_samples))} unique addresses...")
    # Build address remaps: default covers ITCM alias.
    # Can be extended with --itcm-offset for non-standard configurations.
    addr_remaps = SymbolResolver.DEFAULT_REMAPS[:]
    if args.itcm_offset != 0x00200000:
        # User overrode the ITCM offset: replace the default remap
        addr_remaps = [(0xFFF00000, 0x00000000, args.itcm_offset)]
    if args.no_remap:
        addr_remaps = []

    sym_map = SymbolResolver(args.elf, args.addr2line,
                             addr_remaps=addr_remaps).resolve_many(list(set(pc_samples)))

    # Build per-PC stats: we need the most common PC value per symbol
    # so we can print a representative address in the report.
    # pc_by_key[key] = most frequent raw PC address for that key
    pc_counter = collections.Counter(pc_samples)  # raw PC counts

    if args.by_line:
        key_fn = lambda pc: f"{sym_map[pc][0]}  {sym_map[pc][1]}"
        header = f"{'%':>6}  {'samples':>8}  {'PC':>10}  function  file:line"
    else:
        key_fn = lambda pc: sym_map[pc][0]
        header = f"{'%':>6}  {'samples':>8}  {'PC':>10}  function"

    # Group raw PCs by resolved key, track most-common PC per key
    key_to_pcs = collections.defaultdict(list)
    for pc in pc_samples:
        key_to_pcs[key_fn(pc)].append(pc)

    counts = collections.Counter(key_fn(pc) for pc in pc_samples)
    total  = len(pc_samples)

    def repr_pc(key):
        """Most frequent raw PC for this symbol key."""
        return max(collections.Counter(key_to_pcs[key]).items(), key=lambda x: x[1])[0]

    print()
    print("=" * 80)
    print(f"  PC SAMPLING PROFILE  —  {args.elf}")
    print(f"  {total} samples / {elapsed:.1f}s  |  "
          f"baud={actual_baud/1e6:.3f}Mbaud  "
          f"post_tap={args.post_tap}  post_reset={args.post_reset}  "
          f"target={regmap.NAME}")
    print("=" * 80)
    print(header)
    print("-" * 80)

    shown = 0
    for key, count in counts.most_common(args.top_n):
        pc_repr = repr_pc(key)
        print(f"{100.0*count/total:6.2f}%  {count:8d}  0x{pc_repr:08x}  {key}")
        shown += count
    rem = total - shown
    if rem > 0:
        print(f"  ...  ({rem} samples in {len(counts)-args.top_n} more entries)")
    print("-" * 80)
    print(f"{'100.00%':>6}  {total:8d}  {'':10}  TOTAL\n")

    if args.by_line:
        print("Per-function summary:")
        for fn, cnt in collections.Counter(sym_map[pc][0] for pc in pc_samples).most_common(20):
            print(f"  {100.0*cnt/total:6.2f}%  {cnt:8d}  {fn}")

    if not args.by_line:
        print("Top 5 functions — line breakdown:")
        for fn in [k for k, _ in counts.most_common(5)]:
            fps = [pc for pc in pc_samples if sym_map[pc][0] == fn]
            ft  = len(fps)
            print(f"\n  {fn}  ({100.0*ft/total:.2f}%, {ft} samples)")
            for loc, cnt in collections.Counter(sym_map[pc][1] for pc in fps).most_common(10):
                # find a representative PC for this file:line
                line_pcs = [pc for pc in fps if sym_map[pc][1] == loc]
                lpc = max(collections.Counter(line_pcs).items(), key=lambda x: x[1])[0]
                print(f"    {100.0*cnt/ft:6.2f}%  {cnt:6d}  0x{lpc:08x}  {loc}")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    p = argparse.ArgumentParser(
        description="SWO PC-sampling profiler — MIMXRT1170 / MIMXRT1060",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    p.add_argument("--device",     default="MIMXRT1176xxx8_M7",
                   help="J-Link device string")
    p.add_argument("--elf",        required=True,
                   help="ELF with debug symbols")
    p.add_argument("--cpu-freq",   type=int, default=None,
                   help="Core clock Hz (display only). Default: 996000000/600000000")
    p.add_argument("--trace-clk",  type=int, default=None,
                   help="Trace peripheral clock Hz. RT1170 default: 132000000")
    p.add_argument("--prescaler",  type=int, default=None,
                   help="Divisor for SWO_CODR/TPIU_ACPR. "
                        "baud=trace_clk/(N+1). RT1170 default: 99 (→1.32Mbaud). "
                        "Overrides --swo-freq.")
    p.add_argument("--swo-freq",   type=int, default=2_000_000,
                   help="Desired baud Hz (RT1060, ignored if --prescaler given)")
    p.add_argument("--duration",   type=float, default=10.0)
    p.add_argument("--post-reset", type=int, default=15,
                   choices=range(1, 16), metavar="1..15",
                   help="DWT POSTPRESET: 1=fastest, 15=slowest")
    p.add_argument("--post-tap",   type=int, default=1, choices=[0, 1],
                   help="DWT CYCTAP: 0=/64, 1=/1024")
    p.add_argument("--top-n",      type=int, default=40)
    p.add_argument("--by-line",    action="store_true")
    p.add_argument("--output",     default=None)
    p.add_argument("--addr2line",  default="arm-none-eabi-addr2line")
    p.add_argument("--itcm-offset", type=lambda x: int(x,0), default=0x00200000,
                   help="Offset added to unresolved 0x000xxxxx PC addresses for "
                        "ITCM alias remapping (default: 0x200000 for RT1170/RT1060). "
                        "Use 0 to disable.")
    p.add_argument("--no-remap", action="store_true",
                   help="Disable ITCM alias remapping entirely")

    args = p.parse_args()
    run_profiler(args)


if __name__ == "__main__":
    main()