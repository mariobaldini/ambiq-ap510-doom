STACK_SIZE       = 32K;
HEAP_SIZE        = 320K;
GPU_POOL_SIZE    = 1408K;
ZONE_MIN         = 1M;

MEMORY
{
    ITCM  (rwx) : ORIGIN = 0x00000000, LENGTH = 256K
    MRAM  (rx)  : ORIGIN = 0x00410000, LENGTH = 640K
    WAD   (r)   : ORIGIN = 0x004B0000, LENGTH = 0x350000
    DTCM  (rw)  : ORIGIN = 0x20000000, LENGTH = 512K
    SSRAM (rw)  : ORIGIN = 0x20080000, LENGTH = 3M
}

ENTRY(Reset_Handler)

__wad_start = ORIGIN(WAD);
__wad_end   = ORIGIN(WAD) + LENGTH(WAD);

SECTIONS
{
    .text :
    {
        KEEP(*(.vectors))
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > MRAM

    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > MRAM

    .itcm : ALIGN(4)
    {
        __itcm_start = .;
        *(.itcm_text .itcm_text.*)
        . = ALIGN(4);
        __itcm_end = .;
    } > ITCM AT > MRAM
    __itcm_load = LOADADDR(.itcm);

    .data : ALIGN(4)
    {
        __data_start = .;
        *(.data .data.*)
        . = ALIGN(4);
        __data_end = .;
    } > DTCM AT > MRAM
    __data_load = LOADADDR(.data);

    .bss (NOLOAD) : ALIGN(4)
    {
        __bss_start = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > DTCM

    .stack (NOLOAD) : ALIGN(8)
    {
        __StackLimit = .;
        . += STACK_SIZE;
        __StackTop = .;
    } > DTCM

    .gpu_pool (NOLOAD) : ALIGN(32)
    {
        __gpu_pool_start = .;
        . += GPU_POOL_SIZE;
        __gpu_pool_end = .;
    } > SSRAM

    .zone (NOLOAD) : ALIGN(32)
    {
        __zone_start = .;
        . = ORIGIN(SSRAM) + LENGTH(SSRAM) - HEAP_SIZE;
        __zone_end = .;
    } > SSRAM

    .heap (NOLOAD) :
    {
        __heap_start = .;
        end = .;
        . += HEAP_SIZE;
        __heap_end = .;
    } > SSRAM
}

ASSERT(__zone_end - __zone_start >= ZONE_MIN, "Doom zone below ZONE_MIN")
