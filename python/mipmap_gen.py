#!/usr/bin/env python3
"""
mipmap_gen.py  –  Generate mipmapped, palettized textures compatible with the
                  blib3d/texture binary format.

Usage:
    python mipmap_gen.py input.png output.tex [--alpha FLOAT] [--bottom-up] [--no-mip] [--save-png] [--save-cpp]

Dependencies:
    pip install pillow numpy

Format produced (little-endian, matches texture::save()):
    uint16_t  transparency   (0=none, 1=indexed, 2=alpha)
    uint16_t  mipchain       (0=none, 1=enabled)
    int32_t   width          (power-of-2 width of mip level 0)
    int32_t   height         (power-of-2 height of mip level 0)
    uint32_t  size           (total bytes of pixel data)
    ARGB[256] lut            (1024 bytes: B G R A per entry)
    uint8_t[size] data       (palette indices, mip0 then mip1 …)
"""

import argparse
import struct
import sys
from pathlib import Path
from typing import Optional

import numpy as np
from PIL import Image

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def next_power_of_2(n: int) -> int:
    if n <= 0:
        return 1
    p = 1
    while p < n:
        p <<= 1
    return p


def has_alpha(pixels: np.ndarray) -> bool:
    """Return True if any pixel has alpha != 255."""
    if pixels.shape[2] < 4:
        return False
    return bool((pixels[:, :, 3] != 255).any())


# ---------------------------------------------------------------------------
# Median-cut quantisation (faithful port of the C++ implementation)
# ---------------------------------------------------------------------------

def _median_cut_rgb(colors: np.ndarray, begin: int, end: int,
                    depth: int, max_depth: int,
                    lut: list) -> None:
    """
    Recursively split the color range using median cut and append averaged
    palette entries to `lut`.

    colors : (N, 3) uint32 array  (values 0-255 per channel, stored as int)
    """
    count = end - begin
    if count == 0:
        return

    if depth == max_depth:
        chunk = colors[begin:end]
        avg = chunk.mean(axis=0).astype(np.uint8)
        # lut entry: [B, G, R, A]
        lut.append([avg[0], avg[1], avg[2], 0xFF])
        return

    chunk = colors[begin:end]
    lo = chunk.min(axis=0)
    hi = chunk.max(axis=0)
    diff = hi - lo
    max_channel = int(np.argmax(diff))

    # sort by the widest channel
    order = np.argsort(colors[begin:end, max_channel], kind='stable')
    colors[begin:end] = colors[begin:end][order]

    half = (begin + end) // 2
    _median_cut_rgb(colors, begin, half, depth + 1, max_depth, lut)
    _median_cut_rgb(colors, half,  end,  depth + 1, max_depth, lut)


def palette_create_rgb(pixels: np.ndarray,
                       alpha_begin: int = 0,
                       alpha_end: int = 256) -> np.ndarray:
    """
    Build a 256-entry BGRA LUT from an RGB/RGBA image considering only pixels
    whose alpha is in [alpha_begin, alpha_end).

    pixels : (H, W, 3|4) uint8
    Returns: (256, 4) uint8  [B, G, R, A]
    """
    h, w = pixels.shape[:2]
    flat = pixels.reshape(-1, pixels.shape[2])

    if pixels.shape[2] == 4:
        alpha = flat[:, 3].astype(np.int32)
        mask = (alpha >= alpha_begin) & (alpha < alpha_end)
        flat = flat[mask]

    # unique RGB triples (drop alpha for this function)
    rgb = flat[:, :3].astype(np.int32)      # B, G, R  (PIL RGBA → B,G,R,A)
    # de-duplicate via structured view
    view = np.ascontiguousarray(rgb).view(
        np.dtype((np.void, rgb.dtype.itemsize * rgb.shape[1])))
    _, idx = np.unique(view, return_index=True)
    colors = rgb[idx].copy()                 # (M, 3)

    lut_list = []
    _median_cut_rgb(colors, 0, len(colors), 0, 8, lut_list)

    lut = np.zeros((256, 4), dtype=np.uint8)
    n = min(len(lut_list), 256)
    for i in range(n):
        lut[i] = lut_list[i]
    return lut


def palette_create_argb(pixels: np.ndarray) -> np.ndarray:
    """
    Build a 256-entry BGRA LUT from an RGBA image, grouping pixels by alpha
    value and running independent median-cut per alpha bucket (port of
    palette_create_ARGB in the C++ source).

    pixels : (H, W, 4) uint8
    Returns: (256, 4) uint8  [B, G, R, A]
    """
    flat = pixels.reshape(-1, 4).astype(np.int32)   # B G R A columns

    # Build per-alpha color sets
    alpha_lut  = np.zeros((256, 256, 4), dtype=np.uint8)
    alpha_count = np.zeros(256, dtype=np.int32)

    total = 0
    for a in range(256):
        mask = flat[:, 3] == a
        if not mask.any():
            continue
        chunk = flat[mask, :3]                       # B G R

        # unique triples
        view = np.ascontiguousarray(chunk).view(
            np.dtype((np.void, chunk.dtype.itemsize * 3)))
        _, idx = np.unique(view, return_index=True)
        colors = chunk[idx].copy()

        lut_list: list = []
        _median_cut_rgb(colors, 0, len(colors), 0, 8, lut_list)
        n = min(len(lut_list), 256)
        alpha_count[a] = n
        for j in range(n):
            entry = lut_list[j]
            alpha_lut[a, j, 0] = entry[0]   # B
            alpha_lut[a, j, 1] = entry[1]   # G
            alpha_lut[a, j, 2] = entry[2]   # R
            alpha_lut[a, j, 3] = a          # use the bucket alpha
        total += n

    lut = np.zeros((256, 4), dtype=np.uint8)

    if total <= 256:
        # copy each per-alpha palette entry directly into the final LUT
        idx = 0
        for a in range(256):
            n = alpha_count[a]
            for j in range(n):
                if idx >= 256:
                    break
                lut[idx] = alpha_lut[a, j]
                idx += 1
    else:
        # average all colors into 256 buckets
        # flatten all (alpha_lut, alpha_count) into a single list
        all_entries = []
        for a in range(256):
            n = alpha_count[a]
            for j in range(n):
                all_entries.append(alpha_lut[a, j].tolist())   # [B,G,R,A]
        all_entries = np.array(all_entries, dtype=np.int32)    # (total, 4)

        for i in range(256):
            j_start = i * total // 256
            j_end   = (i + 1) * total // 256
            if j_end == j_start:
                j_end = j_start + 1
            j_end = min(j_end, total)
            bucket = all_entries[j_start:j_end]
            avg = bucket.mean(axis=0).astype(np.uint8)
            lut[i] = avg

    return lut


# ---------------------------------------------------------------------------
# Nearest-palette lookup
# ---------------------------------------------------------------------------

def palette_nearest_argb_batch(lut: np.ndarray,
                                pixels: np.ndarray) -> np.ndarray:
    """
    Map every pixel in `pixels` to its nearest LUT index (L1 distance on BGRA).

    lut    : (256, 4) uint8  [B, G, R, A]
    pixels : (N, 4)   uint8  [B, G, R, A]
    Returns: (N,) uint8 indices
    """
    # vectorised L1 distance: (N, 256)
    p = pixels.astype(np.int32)[:, np.newaxis, :]    # (N, 1, 4)
    l = lut.astype(np.int32)[np.newaxis, :, :]       # (1, 256, 4)
    dist = np.abs(p - l).sum(axis=2)                 # (N, 256)
    return dist.argmin(axis=1).astype(np.uint8)


def palette_nearest_rgb_batch(lut: np.ndarray,
                               pixels: np.ndarray) -> np.ndarray:
    """
    Same as above but ignores alpha (uses only first 3 channels).
    """
    p = pixels[:, :3].astype(np.int32)[:, np.newaxis, :]
    l = lut[:, :3].astype(np.int32)[np.newaxis, :, :]
    dist = np.abs(p - l).sum(axis=2)
    return dist.argmin(axis=1).astype(np.uint8)


# ---------------------------------------------------------------------------
# Image resampling helpers
# ---------------------------------------------------------------------------

def _to_pil_bgra(pixels: np.ndarray) -> Image.Image:
    """Convert (H, W, 3|4) numpy array [B,G,R,(A)] to a PIL RGBA image."""
    if pixels.shape[2] == 3:
        rgba = np.dstack([pixels[:, :, 2],   # R
                          pixels[:, :, 1],   # G
                          pixels[:, :, 0],   # B
                          np.full(pixels.shape[:2], 255, np.uint8)])
    else:
        rgba = np.dstack([pixels[:, :, 2],   # R
                          pixels[:, :, 1],   # G
                          pixels[:, :, 0],   # B
                          pixels[:, :, 3]])  # A
    return Image.fromarray(rgba.astype(np.uint8), 'RGBA')


def _from_pil_rgba_to_bgra(img: Image.Image) -> np.ndarray:
    """Convert PIL RGBA image to (H, W, 4) numpy array [B, G, R, A]."""
    arr = np.array(img.convert('RGBA'), dtype=np.uint8)  # R G B A
    return np.dstack([arr[:, :, 2],   # B
                      arr[:, :, 1],   # G
                      arr[:, :, 0],   # R
                      arr[:, :, 3]])  # A


def rescale_high_quality(pixels: np.ndarray, new_w: int, new_h: int) -> np.ndarray:
    """Rescale image using PIL LANCZOS (highest quality)."""
    pil = _to_pil_bgra(pixels)
    pil = pil.resize((new_w, new_h), Image.LANCZOS)
    return _from_pil_rgba_to_bgra(pil)


# ---------------------------------------------------------------------------
# Main mipchain builder
# ---------------------------------------------------------------------------

def _indices_to_png(indices: np.ndarray, lut: np.ndarray,
                    w: int, h: int, bottom_up: bool) -> Image.Image:
    """
    Reconstruct a quantized mip level back to RGBA and return a PIL image.
    Indices are already stored in the bottom-up scanline order used by `data`,
    so we reconstruct them top-down for a normal PNG.
    """
    # indices are written scanline by scanline with optional bottom-up flip;
    # reconstruct the flat top-down order for display
    idx_2d = np.zeros((h, w), dtype=np.uint8)
    for y in range(h):
        yt = (h - 1 - y) if bottom_up else y
        idx_2d[yt] = indices[y * w: y * w + w]

    flat_idx = idx_2d.reshape(-1)
    bgra = lut[flat_idx]                           # (N, 4)  B G R A
    rgba = np.dstack([bgra[:, 2].reshape(h, w),    # R
                      bgra[:, 1].reshape(h, w),    # G
                      bgra[:, 0].reshape(h, w),    # B
                      bgra[:, 3].reshape(h, w)])   # A
    return Image.fromarray(rgba.astype(np.uint8), 'RGBA')


def create_mipchain(
        input_path: str,
        output_path: str,
        alpha_scale: float = 1.0,
        bottom_up: bool = False,
        enable_mip: bool = True,
        png_base_path: Optional[str] = None,
        cpp_base_path: Optional[str] = None) -> None:

    # --- load image ----------------------------------------------------------
    src = Image.open(input_path).convert('RGBA')
    orig_w, orig_h = src.size

    mip_w = next_power_of_2(orig_w)
    mip_h = next_power_of_2(orig_h)

    print(f"Source : {orig_w}×{orig_h}")
    print(f"Mip  0 : {mip_w}×{mip_h}")

    # resize to power-of-2 if needed
    if mip_w != orig_w or mip_h != orig_h:
        src = src.resize((mip_w, mip_h), Image.LANCZOS)

    pixels = _from_pil_rgba_to_bgra(src)  # B G R A

    # optionally scale alpha
    if alpha_scale != 1.0:
        pixels[:, :, 3] = np.clip(
            pixels[:, :, 3].astype(np.float32) * alpha_scale, 0, 255
        ).astype(np.uint8)

    # --- decide transparency mode --------------------------------------------
    if has_alpha(pixels):
        transparency = 2  # transparency_alpha
    else:
        transparency = 0  # transparency_none

    # --- build palette -------------------------------------------------------
    print("Building ARGB palette …")
    if transparency == 2:
        lut = palette_create_argb(pixels)
    else:
        lut = palette_create_rgb(pixels)

    # --- quantize & build mip levels -----------------------------------------
    mipchain_flag = 1 if enable_mip else 0

    # compute total data size
    total_size = 0
    w, h = mip_w, mip_h
    while True:
        total_size += w * h
        if w == 1 or h == 1:
            break
        if not enable_mip:
            break
        w >>= 1
        h >>= 1

    data = bytearray(total_size)
    offset = 0
    current = pixels.copy()
    w, h = mip_w, mip_h
    level = 0

    while True:
        print(f"  Quantizing mip {level}: {w}×{h} …")
        flat = current.reshape(-1, 4)           # (w*h, 4) BGRA

        if transparency == 2:
            indices = palette_nearest_argb_batch(lut, flat)
        else:
            indices = palette_nearest_rgb_batch(lut, flat)

        # write scanlines (optionally bottom-up)
        for y in range(h):
            yt = (h - 1 - y) if bottom_up else y
            row = indices[yt * w: yt * w + w]
            data[offset: offset + w] = row.tobytes()
            offset += w

        # optionally save a preview PNG for this mip level
        if png_base_path is not None:
            base = Path(png_base_path)
            png_path = (
                base.parent / f"{base.stem}_mip{level}{base.suffix}"
                if enable_mip else base
            )
            preview = _indices_to_png(indices, lut, w, h, bottom_up)
            preview.save(str(png_path))
            print(f"    Saved preview → {png_path}")

        if w == 1 or h == 1:
            break
        if not enable_mip:
            break

        w >>= 1
        h >>= 1
        level += 1
        pil_next = _to_pil_bgra(current).resize((w, h), Image.LANCZOS)
        current = _from_pil_rgba_to_bgra(pil_next)

    assert offset == total_size, f"size mismatch: {offset} != {total_size}"

    # --- write binary file ---------------------------------------------------
    # Header:
    #   uint16  transparency
    #   uint16  mipchain
    #   int32   width
    #   int32   height
    #   uint32  size
    #   ARGB[256] lut   (256 × 4 bytes = 1024 bytes, order B G R A)
    #   uint8[size] data

    with open(output_path, 'wb') as f:
        f.write(struct.pack('<HHiiI', transparency, mipchain_flag,
                            mip_w, mip_h, total_size))
        # LUT: 256 entries of 4 bytes each (B, G, R, A)
        f.write(lut.tobytes())            # already (256,4) uint8
        f.write(bytes(data))

    kb = (14 + 1024 + total_size) / 1024
    print(f"\nWrote '{output_path}'  ({kb:.1f} KB)")
    print(f"  transparency={transparency}, mipchain={mipchain_flag}, "
          f"width={mip_w}, height={mip_h}, size={total_size}")

    # --- optionally export C++ sources ---------------------------------------
    if cpp_base_path is not None:
        symbol = Path(cpp_base_path).stem
        # sanitize: replace non-identifier chars with underscore
        symbol = ''.join(c if c.isalnum() or c == '_' else '_' for c in symbol)
        if symbol and symbol[0].isdigit():
            symbol = '_' + symbol
        print(f"\nExporting C++ sources (symbol: {symbol}) …")
        export_cpp(
            cpp_base_path=cpp_base_path,
            symbol_name=symbol,
            lut=lut,
            pixel_data=bytes(data),
            transparency=transparency,
            mipchain_flag=mipchain_flag,
            mip_w=mip_w,
            mip_h=mip_h,
            total_size=total_size,
        )


# ---------------------------------------------------------------------------
# C++ source export
# ---------------------------------------------------------------------------

def _cpp_hex_array(data: bytes, indent: str = '    ', cols: int = 16) -> str:
    """Format a bytes object as a C hex literal array body (no braces)."""
    lines = []
    for i in range(0, len(data), cols):
        chunk = data[i:i + cols]
        lines.append(indent + ', '.join(f'0x{b:02x}u' for b in chunk))
    return ',\n'.join(lines)


def export_cpp(
        cpp_base_path: str,
        symbol_name: str,
        lut: np.ndarray,
        pixel_data: bytes,
        transparency: int,
        mipchain_flag: int,
        mip_w: int,
        mip_h: int,
        total_size: int) -> None:
    """
    Write <cpp_base_path>.hpp and <cpp_base_path>.cpp that embed the texture
    as static const data and expose it as a const texture::texture& reference.

    The struct layout matches texture::texture exactly:
        uint16_t  transparency
        uint16_t  mipchain
        int32_t   width
        int32_t   height
        uint32_t  size
        ARGB*     lut    → points into s_lut[]
        uint8_t*  data   → points into s_data[]

    Because lut/data are raw pointers in the struct we cannot make the
    texture itself constexpr, but we can make the backing arrays const and
    initialise a non-const texture wrapper that borrows them.  To keep
    things simple and safe the .cpp exposes a plain `const texture::texture`
    whose lut/data pointers are set in a small init helper called once via a
    static initialiser.
    """
    base   = Path(cpp_base_path)
    hpp_path = base.with_suffix('.hpp')
    cpp_path = base.with_suffix('.cpp')

    guard = symbol_name.upper() + '_HPP'

    # ---- .hpp ---------------------------------------------------------------
    hpp = f"""\
#pragma once
// Auto-generated by mipmap_gen.py – do not edit.
#ifndef {guard}
#define {guard}

#include <cstdint>

namespace texture {{

struct ARGB
{{
    uint8_t b, g, r, a;
}};

struct texture
{{
    uint16_t transparency{{ 0 }};
    uint16_t mipchain{{ 0 }};
    int32_t  width{{ 0 }};
    int32_t  height{{ 0 }};
    uint32_t size{{ 0 }};
    ARGB*    lut{{ nullptr }};
    uint8_t* data{{ nullptr }};
}};

}} // namespace texture

// Texture: {symbol_name}
//   {mip_w}x{mip_h}, transparency={transparency}, mipchain={mipchain_flag}
//   palette: 256 ARGB entries, pixel data: {total_size} bytes
extern const texture::texture {symbol_name};

#endif // {guard}
"""

    # ---- .cpp ---------------------------------------------------------------
    lut_bytes   = lut.tobytes()          # 256 * 4 = 1024 bytes, order B G R A
    lut_comment = f"// LUT: 256 ARGB entries ({len(lut_bytes)} bytes)"
    dat_comment = (f"// Pixel data: {total_size} bytes  "
                   f"({'mipchain' if mipchain_flag else 'single level'},"
                   f" {'bottom-up' if False else 'top-down'} scanlines)")

    cpp = f"""\
// Auto-generated by mipmap_gen.py – do not edit.
#include "{hpp_path.name}"

namespace {{

{lut_comment}
static const uint8_t s_lut[{len(lut_bytes)}] = {{
{_cpp_hex_array(lut_bytes)}
}};

{dat_comment}
static const uint8_t s_data[{total_size}] = {{
{_cpp_hex_array(pixel_data)}
}};

// Initialise the texture struct borrowing the static arrays above.
// The const_cast is intentional: the struct stores non-const pointers but
// we never write through them after construction.
static texture::texture make_{symbol_name}()
{{
    texture::texture t;
    t.transparency = {transparency}u;
    t.mipchain     = {mipchain_flag}u;
    t.width        = {mip_w};
    t.height       = {mip_h};
    t.size         = {total_size}u;
    t.lut          = const_cast<texture::ARGB*>(
                         reinterpret_cast<const texture::ARGB*>(s_lut));
    t.data         = const_cast<uint8_t*>(s_data);
    return t;
}}

}} // anonymous namespace

const texture::texture {symbol_name} = make_{symbol_name}();
"""

    hpp_path.write_text(hpp, encoding='utf-8')
    cpp_path.write_text(cpp, encoding='utf-8')
    print(f"  Wrote C++ header → {hpp_path}")
    print(f"  Wrote C++ source → {cpp_path}")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description='Generate a palettized mipchain texture (blib3d format).')
    parser.add_argument('input',  help='Input image (PNG, JPEG, …)')
    parser.add_argument('output', help='Output .tex binary file')
    parser.add_argument('--alpha', type=float, default=1.0,
                        help='Alpha multiplier applied to every pixel (default: 1.0)')
    parser.add_argument('--bottom-up', action='store_true',
                        help='Store scanlines bottom-up (like OpenGL textures)')
    parser.add_argument('--no-mip', action='store_true',
                        help='Write only the full-resolution level (no mipchain)')
    parser.add_argument('--save-png', metavar='PATH',
                        help='Also save quantized mip levels as PNG files. '
                             'With mipchain: output is PATH_mip0.png, PATH_mip1.png, … '
                             '(PATH stem + suffix, e.g. preview.png). '
                             'Without mipchain: saved directly as PATH.')
    parser.add_argument('--save-cpp', metavar='BASE',
                        help='Export texture as C++ source files BASE.hpp / BASE.cpp. '
                             'The symbol name is derived from the BASE filename stem '
                             '(e.g. --save-cpp textures/my_tex → symbol my_tex).')
    args = parser.parse_args()

    if not Path(args.input).exists():
        sys.exit(f"Error: input file '{args.input}' not found.")

    create_mipchain(
        input_path=args.input,
        output_path=args.output,
        alpha_scale=args.alpha,
        bottom_up=args.bottom_up,
        enable_mip=not args.no_mip,
        png_base_path=args.save_png,
        cpp_base_path=args.save_cpp,
    )


if __name__ == '__main__':
    main()
