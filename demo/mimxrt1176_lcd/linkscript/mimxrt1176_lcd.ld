
GROUP (
  "libgcc.a"
  "libc_nano.a"
  "libstdc++_nano.a"
  "libm.a"
  "libcr_newlib_semihost.a"
  "crti.o"
  "crtn.o"
  "crtbegin.o"
  "crtend.o"
)

MEMORY
{
  /* Define each memory region */
  BOARD_FLASH (rx)    : ORIGIN = 0x30000000, LENGTH = 0x4000000 /* 64M bytes (alias Flash) */  
  BOARD_SDRAM (rwx)   : ORIGIN = 0x80000000, LENGTH = 0x3000000 /* 48M bytes (alias RAM) */  
  NCACHE_REGION (rwx) : ORIGIN = 0x83000000, LENGTH = 0x1000000 /* 16M bytes (alias RAM2) */  
  SRAM_DTC_cm7 (rwx)  : ORIGIN = 0x20000000, LENGTH = 0x40000 /* 256K bytes (alias RAM3) */  
  SRAM_ITC_cm7 (rwx)  : ORIGIN = 0x00000000, LENGTH = 0x40000 /* 256K bytes (alias RAM4) */  
  SRAM_OC (rwx)       : ORIGIN = 0x20240000, LENGTH = 0x140000 /* 1280K bytes (alias RAM5) */  
}

/* Define a symbol for the top of each memory region */
__base_BOARD_FLASH = 0x30000000  ; /* BOARD_FLASH */  
__base_Flash = 0x30000000 ; /* Flash */  
__top_BOARD_FLASH = 0x30000000 + 0x4000000 ; /* 64M bytes */  
__top_Flash = 0x30000000 + 0x4000000 ; /* 64M bytes */  
__base_BOARD_SDRAM = 0x80000000  ; /* BOARD_SDRAM */  
__base_RAM = 0x80000000 ; /* RAM */  
__top_BOARD_SDRAM = 0x80000000 + 0x3000000 ; /* 48M bytes */  
__top_RAM = 0x80000000 + 0x3000000 ; /* 48M bytes */  
__base_NCACHE_REGION = 0x83000000  ; /* NCACHE_REGION */  
__base_RAM2 = 0x83000000 ; /* RAM2 */  
__top_NCACHE_REGION = 0x83000000 + 0x1000000 ; /* 16M bytes */  
__top_RAM2 = 0x83000000 + 0x1000000 ; /* 16M bytes */  
__base_SRAM_DTC_cm7 = 0x20000000  ; /* SRAM_DTC_cm7 */  
__base_RAM3 = 0x20000000 ; /* RAM3 */  
__top_SRAM_DTC_cm7 = 0x20000000 + 0x40000 ; /* 256K bytes */  
__top_RAM3 = 0x20000000 + 0x40000 ; /* 256K bytes */  
__base_SRAM_ITC_cm7 = 0x0  ; /* SRAM_ITC_cm7 */  
__base_RAM4 = 0x0 ; /* RAM4 */  
__top_SRAM_ITC_cm7 = 0x0 + 0x40000 ; /* 256K bytes */  
__top_RAM4 = 0x0 + 0x40000 ; /* 256K bytes */  
__base_SRAM_OC = 0x20240000  ; /* SRAM_OC */  
__base_RAM5 = 0x20240000 ; /* RAM5 */  
__top_SRAM_OC = 0x20240000 + 0x140000 ; /* 1280K bytes */  
__top_RAM5 = 0x20240000 + 0x140000 ; /* 1280K bytes */  

ENTRY(ResetISR)

SECTIONS
{
     /* Image Vector Table and Boot Data for booting from external flash */
    .boot_hdr : ALIGN(4)
    {
        FILL(0xff)
        . = 0x400 ;
        __boot_hdr_start__ = ABSOLUTE(.) ;
        KEEP(*(.boot_hdr.conf))
        . = 0x1000 ;
        __boot_hdr_ivt_loadaddr__  =  ABSOLUTE(.) ;
        KEEP(*(.boot_hdr.ivt))
        . = 0x1020 ;
        __boot_hdr_boot_data_loadaddr__ = ABSOLUTE(.) ;
        KEEP(*(.boot_hdr.boot_data))
        . = 0x1030 ;
        __boot_hdr_dcd_loadaddr__ = ABSOLUTE(.) ;
        KEEP(*(.boot_hdr.dcd_data))
        __boot_hdr_end__ = ABSOLUTE(.) ;
        . = 0x2000 ;
    } >BOARD_FLASH
    
    .vectors : ALIGN(4)
    {
        FILL(0xff)
        __vectors_start__ = ABSOLUTE(.) ;
        KEEP(*(.isr_vector))
        /* Global Section Table */
        . = ALIGN(4) ;
        __section_table_start = .;
        __data_section_table = .;
        LONG(LOADADDR(.text_RAM));
        LONG(    ADDR(.text_RAM));
        LONG(  SIZEOF(.text_RAM));
        LONG(LOADADDR(.text_RAM2));
        LONG(    ADDR(.text_RAM2));
        LONG(  SIZEOF(.text_RAM2));
        LONG(LOADADDR(.text_RAM3));
        LONG(    ADDR(.text_RAM3));
        LONG(  SIZEOF(.text_RAM3));
        LONG(LOADADDR(.text_RAM4));
        LONG(    ADDR(.text_RAM4));
        LONG(  SIZEOF(.text_RAM4));
        LONG(LOADADDR(.text_RAM5));
        LONG(    ADDR(.text_RAM5));
        LONG(  SIZEOF(.text_RAM5));
        LONG(LOADADDR(.data));
        LONG(    ADDR(.data));
        LONG(  SIZEOF(.data));
        LONG(LOADADDR(.data_RAM2));
        LONG(    ADDR(.data_RAM2));
        LONG(  SIZEOF(.data_RAM2));
        LONG(LOADADDR(.data_RAM3));
        LONG(    ADDR(.data_RAM3));
        LONG(  SIZEOF(.data_RAM3));
        LONG(LOADADDR(.data_RAM4));
        LONG(    ADDR(.data_RAM4));
        LONG(  SIZEOF(.data_RAM4));
        LONG(LOADADDR(.data_RAM5));
        LONG(    ADDR(.data_RAM5));
        LONG(  SIZEOF(.data_RAM5));
        __data_section_table_end = .;
        __bss_section_table = .;
        LONG(    ADDR(.bss));
        LONG(  SIZEOF(.bss));
        LONG(    ADDR(.bss_RAM2));
        LONG(  SIZEOF(.bss_RAM2));
        LONG(    ADDR(.bss_RAM3));
        LONG(  SIZEOF(.bss_RAM3));
        LONG(    ADDR(.bss_RAM4));
        LONG(  SIZEOF(.bss_RAM4));
        LONG(    ADDR(.bss_RAM5));
        LONG(  SIZEOF(.bss_RAM5));
        __bss_section_table_end = .;
        __section_table_end = . ;
        /* End of Global Section Table */

        *(.after_vectors*)
    } > BOARD_FLASH

    .ctors : ALIGN(4)
    {
        FILL(0xff)

        /* C++ constructors etc */
        . = ALIGN(4);
        KEEP(*(.init))
        
        . = ALIGN(4);
        __preinit_array_start = .;
        KEEP (*(.preinit_array))
        __preinit_array_end = .;
        
        . = ALIGN(4);
        __init_array_start = .;
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array))
        __init_array_end = .;
        
        KEEP(*(.fini));
        
        . = ALIGN(4);
        KEEP (*crtbegin.o(.ctors))
        KEEP (*(EXCLUDE_FILE (*crtend.o) .ctors))
        KEEP (*(SORT(.ctors.*)))
        KEEP (*crtend.o(.ctors))
        
        . = ALIGN(4);
        KEEP (*crtbegin.o(.dtors))
        KEEP (*(EXCLUDE_FILE (*crtend.o) .dtors))
        KEEP (*(SORT(.dtors.*)))
        KEEP (*crtend.o(.dtors))
        . = ALIGN(4);
        /* End C++ */
    } > BOARD_FLASH
    
    /*
     * for exception handling/unwind - some Newlib functions (in common
     * with C++ and STDC++) use this.
     */
    .ARM.extab : ALIGN(4)
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > BOARD_FLASH

    .ARM.exidx : ALIGN(4)
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > BOARD_FLASH
    
    /* TEXT section for BOARD_SDRAM */
    
    .text_RAM : ALIGN(4)
    {
        FILL(0xff)
        PROVIDE(__start_text_RAM = .) ;
        PROVIDE(__start_text_BOARD_SDRAM = .) ;
        *(.text.$RAM)
        *(.text.$BOARD_SDRAM)
        *(.text.$RAM.*)
        *(.text.$BOARD_SDRAM.*)
        . = ALIGN(4) ;
        PROVIDE(__end_text_RAM = .) ;
        PROVIDE(__end_text_BOARD_SDRAM = .) ;
    } > BOARD_SDRAM AT>BOARD_FLASH
    
    /* TEXT section for NCACHE_REGION */
    
    .text_RAM2 : ALIGN(4)
    {
        FILL(0xff)
        PROVIDE(__start_text_RAM2 = .) ;
        PROVIDE(__start_text_NCACHE_REGION = .) ;
        *(.ramfunc.$RAM2)
        *(.ramfunc.$NCACHE_REGION)
        *(NonCacheable.init)
        *(.text.$RAM2)
        *(.text.$NCACHE_REGION)
        *(.text.$RAM2.*)
        *(.text.$NCACHE_REGION.*)
        . = ALIGN(4) ;
        PROVIDE(__end_text_RAM2 = .) ;
        PROVIDE(__end_text_NCACHE_REGION = .) ;
    } > NCACHE_REGION AT>BOARD_FLASH

    /* TEXT section for SRAM_DTC_cm7 */

    .text_RAM3 : ALIGN(4)
    {
        FILL(0xff)
        PROVIDE(__start_text_RAM3 = .) ;
        PROVIDE(__start_text_SRAM_DTC_cm7 = .) ;
        *(.ramfunc.$RAM3)
        *(.ramfunc.$SRAM_DTC_cm7)
        *(.text.$RAM3)
        *(.text.$SRAM_DTC_cm7)
        *(.text.$RAM3.*)
        *(.text.$SRAM_DTC_cm7.*)
        
        *(.rodata.brick_lut)
        *(.rodata.brick_width)
        *(.rodata.brick_height)
        
        . = ALIGN(4) ;
        PROVIDE(__end_text_RAM3 = .) ;
        PROVIDE(__end_text_SRAM_DTC_cm7 = .) ;
    } > SRAM_DTC_cm7 AT>BOARD_FLASH

    /* TEXT section for SRAM_ITC_cm7 */

    .text_RAM4 : ALIGN(4)
    {
        FILL(0xff)
        PROVIDE(__start_text_RAM4 = .) ;
        PROVIDE(__start_text_SRAM_ITC_cm7 = .) ;
        *(.ramfunc.$RAM4)
        *(.ramfunc.$SRAM_ITC_cm7)
        *(.text.$RAM4)
        *(.text.$SRAM_ITC_cm7)
        *(.text.$RAM4.*)
        *(.text.$SRAM_ITC_cm7.*)
        
        *(.rodata.brick_data)
        
        . = ALIGN(4) ;
        PROVIDE(__end_text_RAM4 = .) ;
        PROVIDE(__end_text_SRAM_ITC_cm7 = .) ;
    } > SRAM_ITC_cm7 AT>BOARD_FLASH

    /* TEXT section for SRAM_OC */

    .text_RAM5 : ALIGN(4)
    {
        FILL(0xff)
        PROVIDE(__text_data_RAM5 = .) ;
        PROVIDE(__text_data_SRAM_OC = .) ;
        *(.ramfunc.$RAM5)
        *(.ramfunc.$SRAM_OC)
        *(.text.$RAM5)
        *(.text.$SRAM_OC)
        *(.text.$RAM5.*)
        *(.text.$SRAM_OC.*)
        *(.ramfunc*)
        *(RamFunction)
        KEEP(*(CodeQuickAccess))
        
        *elcdif_rgb*(*.text *.text.*)
        *elcdif_rgb*(*.rodata *.rodata.*)

        *source/math*(*.text *.text.*)
        *source/math*(*.rodata *.rodata.*)
        *source/raster*(*.text *.text.*)
        *source/raster*(*.rodata *.rodata.*)
        *source/render*(*.text *.text.*)
        *source/render*(*.rodata *.rodata.*)
        *source/timer*(*.text *.text.*)
        *source/timer*(*.rodata *.rodata.*)
        *source/view*(*.text *.text.*)
        *source/view*(*.rodata *.rodata.*)
        
        *source/brick*(*.text *.text.*)
        *source/brick*(*.rodata *.rodata.*)
        *source/demo*(*.text *.text.*)
        *source/demo*(*.rodata *.rodata.*)
        *source/draw2d*(*.text *.text.*)
        *source/draw2d*(*.rodata *.rodata.*)
        *source/rect_model*(*.text *.text.*)
        *source/rect_model*(*.rodata *.rodata.*)
        *source/text_model*(*.text *.text.*)
        *source/text_model*(*.rodata *.rodata.*)
        *source/triangle*(*.text *.text.*)
        *source/triangle*(*.rodata *.rodata.*)
        
        . = ALIGN(4) ;
        PROVIDE(__end_text_RAM5 = .) ;
        PROVIDE(__end_text_SRAM_OC = .) ;
    } > SRAM_OC AT>BOARD_FLASH
    
    /* MAIN TEXT SECTION */
    .text : ALIGN(4)
    {
        FILL(0xff)

        *(.text*)
        *(.rodata .rodata.* .constdata .constdata.*)
        . = ALIGN(4);
    } > BOARD_FLASH
 
	_etext = .;
        
    /* DATA section for NCACHE_REGION */

    .data_RAM2 : ALIGN(4)
    {
        FILL(0xff)
        PROVIDE(__start_data_RAM2 = .) ;
        PROVIDE(__start_data_NCACHE_REGION = .) ;
        *(.data.$RAM2)
        *(.data.$NCACHE_REGION)
        *(.data.$RAM2.*)
        *(.data.$NCACHE_REGION.*)
        . = ALIGN(4) ;
        PROVIDE(__end_data_RAM2 = .) ;
        PROVIDE(__end_data_NCACHE_REGION = .) ;
    } > NCACHE_REGION AT>BOARD_FLASH

    /* DATA section for SRAM_DTC_cm7 */

    .data_RAM3 : ALIGN(4)
    {
        FILL(0xff)
        PROVIDE(__start_data_RAM3 = .) ;
        PROVIDE(__start_data_SRAM_DTC_cm7 = .) ;
        *(.data.$RAM3)
        *(.data.$SRAM_DTC_cm7)
        *(.data.$RAM3.*)
        *(.data.$SRAM_DTC_cm7.*)
        
        *source/math*(*.data *.data.*)
        *source/raster*(*.data *.data.*)
        *source/render*(*.data *.data.*)
        *source/timer*(*.data *.data.*)
        *source/view*(*.data *.data.*)
        
        . = ALIGN(4) ;
        PROVIDE(__end_data_RAM3 = .) ;
        PROVIDE(__end_data_SRAM_DTC_cm7 = .) ;
    } > SRAM_DTC_cm7 AT>BOARD_FLASH

    /* DATA section for SRAM_ITC_cm7 */

    .data_RAM4 : ALIGN(4)
    {
        FILL(0xff)
        PROVIDE(__start_data_RAM4 = .) ;
        PROVIDE(__start_data_SRAM_ITC_cm7 = .) ;
        *(.data.$RAM4)
        *(.data.$SRAM_ITC_cm7)
        *(.data.$RAM4.*)
        *(.data.$SRAM_ITC_cm7.*)
        . = ALIGN(4) ;
        PROVIDE(__end_data_RAM4 = .) ;
        PROVIDE(__end_data_SRAM_ITC_cm7 = .) ;
    } > SRAM_ITC_cm7 AT>BOARD_FLASH

    /* DATA section for SRAM_OC */

    .data_RAM5 : ALIGN(4)
    {
        FILL(0xff)
        PROVIDE(__start_data_RAM5 = .) ;
        PROVIDE(__start_data_SRAM_OC = .) ;
        *(.data.$RAM5)
        *(.data.$SRAM_OC)
        *(.data.$RAM5.*)
        *(.data.$SRAM_OC.*)
        *(vtable)
        KEEP(*(DataQuickAccess))
        
        *source/brick*(*.data *.data.*)
        *source/demo*(*.data *.data.*)
        *source/draw2d*(*.data *.data.*)
        *source/rect_model*(*.data *.data.*)
        *source/text_model*(*.data *.data.*)
        *source/triangle*(*.data *.data.*)
        
        . = ALIGN(4) ;
        PROVIDE(__end_data_RAM5 = .) ;
        PROVIDE(__end_data_SRAM_OC = .) ;
    } > SRAM_OC AT>BOARD_FLASH

    /* MAIN DATA SECTION */
    .uninit_RESERVED (NOLOAD) : ALIGN(4)
    {
        _start_uninit_RESERVED = .;
        KEEP(*(.bss.$RESERVED*))
       . = ALIGN(4) ;
        _end_uninit_RESERVED = .;
    } > BOARD_SDRAM AT> BOARD_SDRAM

    /* Main DATA section (BOARD_SDRAM) */
    .data : ALIGN(4)
    {
       FILL(0xff)
       _data = . ;
       PROVIDE(__start_data_RAM = .) ;
       PROVIDE(__start_data_BOARD_SDRAM = .) ;
       *(.data*)
       . = ALIGN(4) ;
       _edata = . ;
       PROVIDE(__end_data_RAM = .) ;
       PROVIDE(__end_data_BOARD_SDRAM = .) ;
    } > BOARD_SDRAM AT>BOARD_FLASH

    /* BSS section for NCACHE_REGION */
    .bss_RAM2 (NOLOAD) : ALIGN(4)
    {
       PROVIDE(__start_bss_RAM2 = .) ;
       PROVIDE(__start_bss_NCACHE_REGION = .) ;
       *(NonCacheable)
       *(.bss.$RAM2)
       *(.bss.$NCACHE_REGION)
       *(.bss.$RAM2.*)
       *(.bss.$NCACHE_REGION.*)
       . = ALIGN (. != 0 ? 4 : 1) ; /* avoid empty segment */
       PROVIDE(__end_bss_RAM2 = .) ;
       PROVIDE(__end_bss_NCACHE_REGION = .) ;
    } > NCACHE_REGION AT> NCACHE_REGION

    /* BSS section for SRAM_DTC_cm7 */
    .bss_RAM3 (NOLOAD) : ALIGN(4)
    {
       PROVIDE(__start_bss_RAM3 = .) ;
       PROVIDE(__start_bss_SRAM_DTC_cm7 = .) ;
       *(.bss.$RAM3)
       *(.bss.$SRAM_DTC_cm7)
       *(.bss.$RAM3.*)
       *(.bss.$SRAM_DTC_cm7.*)
       
       *source/math*(*.bss *.bss.*)
       *source/raster*(*.bss *.bss.*)
       *source/render*(*.bss *.bss.*)
       *source/timer*(*.bss *.bss.*)
       *source/view*(*.bss *.bss.*)

       *(*.bss.*renderer*)
       
       . = ALIGN (. != 0 ? 4 : 1) ; /* avoid empty segment */
       PROVIDE(__end_bss_RAM3 = .) ;
       PROVIDE(__end_bss_SRAM_DTC_cm7 = .) ;
    } > SRAM_DTC_cm7 AT> SRAM_DTC_cm7

    /* BSS section for SRAM_ITC_cm7 */
    .bss_RAM4 (NOLOAD) : ALIGN(4)
    {
       PROVIDE(__start_bss_RAM4 = .) ;
       PROVIDE(__start_bss_SRAM_ITC_cm7 = .) ;
       *(.bss.$RAM4)
       *(.bss.$SRAM_ITC_cm7)
       *(.bss.$RAM4.*)
       *(.bss.$SRAM_ITC_cm7.*)
       . = ALIGN (. != 0 ? 4 : 1) ; /* avoid empty segment */
       PROVIDE(__end_bss_RAM4 = .) ;
       PROVIDE(__end_bss_SRAM_ITC_cm7 = .) ;
    } > SRAM_ITC_cm7 AT> SRAM_ITC_cm7

    /* BSS section for SRAM_OC */
    .bss_RAM5 (NOLOAD) : ALIGN(4)
    {
       PROVIDE(__start_bss_RAM5 = .) ;
       PROVIDE(__start_bss_SRAM_OC = .) ;
       *(.bss.$RAM5)
       *(.bss.$SRAM_OC)
       *(.bss.$RAM5.*)
       *(.bss.$SRAM_OC.*)
       
       *source/brick*(*.bss *.bss.*)
       *source/demo*(*.bss *.bss.*)
       *source/draw2d*(*.bss *.bss.*)
       *source/rect_model*(*.bss *.bss.*)
       *source/text_model*(*.bss *.bss.*)
       *source/triangle*(*.bss *.bss.*)
       
       . = ALIGN (. != 0 ? 4 : 1) ; /* avoid empty segment */
       PROVIDE(__end_bss_RAM5 = .) ;
       PROVIDE(__end_bss_SRAM_OC = .) ;
    } > SRAM_OC AT> SRAM_OC

    /* MAIN BSS SECTION */
    .bss (NOLOAD) : ALIGN(4)
    {
        _bss = .;
        PROVIDE(__start_bss_RAM = .) ;
        PROVIDE(__start_bss_BOARD_SDRAM = .) ;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4) ;
        _ebss = .;
        PROVIDE(__end_bss_RAM = .) ;
        PROVIDE(__end_bss_BOARD_SDRAM = .) ;
        PROVIDE(end = .);
    } > BOARD_SDRAM AT> BOARD_SDRAM

    /* NOINIT section for NCACHE_REGION */
    .noinit_RAM2 (NOLOAD) : ALIGN(4)
    {
       PROVIDE(__start_noinit_RAM2 = .) ;
       PROVIDE(__start_noinit_NCACHE_REGION = .) ;
       *(.noinit.$RAM2)
       *(.noinit.$NCACHE_REGION)
       *(.noinit.$RAM2.*)
       *(.noinit.$NCACHE_REGION.*)
       . = ALIGN(4) ;
       PROVIDE(__end_noinit_RAM2 = .) ;
       PROVIDE(__end_noinit_NCACHE_REGION = .) ;
    } > NCACHE_REGION AT> NCACHE_REGION

    /* NOINIT section for SRAM_DTC_cm7 */
    .noinit_RAM3 (NOLOAD) : ALIGN(4)
    {
       PROVIDE(__start_noinit_RAM3 = .) ;
       PROVIDE(__start_noinit_SRAM_DTC_cm7 = .) ;
       *(.noinit.$RAM3)
       *(.noinit.$SRAM_DTC_cm7)
       *(.noinit.$RAM3.*)
       *(.noinit.$SRAM_DTC_cm7.*)
       . = ALIGN(4) ;
       PROVIDE(__end_noinit_RAM3 = .) ;
       PROVIDE(__end_noinit_SRAM_DTC_cm7 = .) ;
    } > SRAM_DTC_cm7 AT> SRAM_DTC_cm7

    /* NOINIT section for SRAM_ITC_cm7 */
    .noinit_RAM4 (NOLOAD) : ALIGN(4)
    {
       PROVIDE(__start_noinit_RAM4 = .) ;
       PROVIDE(__start_noinit_SRAM_ITC_cm7 = .) ;
       *(.noinit.$RAM4)
       *(.noinit.$SRAM_ITC_cm7)
       *(.noinit.$RAM4.*)
       *(.noinit.$SRAM_ITC_cm7.*)
       . = ALIGN(4) ;
       PROVIDE(__end_noinit_RAM4 = .) ;
       PROVIDE(__end_noinit_SRAM_ITC_cm7 = .) ;
    } > SRAM_ITC_cm7 AT> SRAM_ITC_cm7

    /* NOINIT section for SRAM_OC */
    .noinit_RAM5 (NOLOAD) : ALIGN(4)
    {
       PROVIDE(__start_noinit_RAM5 = .) ;
       PROVIDE(__start_noinit_SRAM_OC = .) ;
       *(.noinit.$RAM5)
       *(.noinit.$SRAM_OC)
       *(.noinit.$RAM5.*)
       *(.noinit.$SRAM_OC.*)
       . = ALIGN(4) ;
       PROVIDE(__end_noinit_RAM5 = .) ;
       PROVIDE(__end_noinit_SRAM_OC = .) ;
    } > SRAM_OC AT> SRAM_OC

    /* DEFAULT NOINIT SECTION */
    .noinit (NOLOAD): ALIGN(4)
    {
        _noinit = .;
        PROVIDE(__start_noinit_RAM = .) ;
        PROVIDE(__start_noinit_BOARD_SDRAM = .) ;
        *(.noinit*)
         . = ALIGN(4) ;
        _end_noinit = .;
       PROVIDE(__end_noinit_RAM = .) ;
       PROVIDE(__end_noinit_BOARD_SDRAM = .) ;        
    } > BOARD_SDRAM AT> BOARD_SDRAM

    /* Reserve and place Heap within memory map */
    _HeapSize = 0x2000;
    .heap (NOLOAD) :  ALIGN(4)
    {
        _pvHeapStart = .;
        . += _HeapSize;
        . = ALIGN(4);
        _pvHeapLimit = .;
    } > BOARD_SDRAM

     _StackSize = 0x2000;
     /* Reserve space in memory for Stack */
    .heap2stackfill (NOLOAD) :
    {
        . += _StackSize;
    } > SRAM_DTC_cm7
    /* Locate actual Stack in memory map */
    .stack ORIGIN(SRAM_DTC_cm7) + LENGTH(SRAM_DTC_cm7) - _StackSize - 0 (NOLOAD) :  ALIGN(4)
    {
        _vStackBase = .;
        . = ALIGN(4);
        _vStackTop = . + _StackSize;
    } > SRAM_DTC_cm7

    /* Provide basic symbols giving location and size of main text
     * block, including initial values of RW data sections. Note that
     * these will need extending to give a complete picture with
     * complex images (e.g multiple Flash banks).
     */
    _image_start = LOADADDR(.text);
    _image_end = LOADADDR(.data) + SIZEOF(.data);
    _image_size = _image_end - _image_start;
    /* Provide symbols for MIMXRT1170 parts for boot header generation code
     * to set image to be plain load image or XIP.
     * Config : Plain load image = false
     */
    _boot_loadaddr = ORIGIN(BOARD_FLASH);
    _boot_size = LENGTH(BOARD_FLASH);
}
