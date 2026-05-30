/* GNU linker script for the GigaDevice GD32E503CE (single-image dev build).
 *   Flash : 256 KB @ 0x08000000
 *   SRAM  :  64 KB @ 0x20000000
 * Same SECTIONS body as the GD32E517RE port; only the memory regions differ. */

ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 256K
    SRAM  (xrw) : ORIGIN = 0x20000000, LENGTH = 64K
}

_vtor_base      = 0x08000000;
_estack         = ORIGIN(SRAM) + LENGTH(SRAM);
_Min_Heap_Size  = 0x400;
_Min_Stack_Size = 0x1000;

INCLUDE papr_sections.ld
