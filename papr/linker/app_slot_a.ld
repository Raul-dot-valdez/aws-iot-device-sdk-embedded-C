/* GNU linker script — PAPR application in SLOT A (GD32E517RE).
 *
 *   slot A app  : 0x08008000  238 KB
 *   manifest    : 0x08043800    2 KB  (signed trailer, written by OTA commit)
 *
 * The image is linked to run from 0x08008000. The startup / SystemInit must
 * relocate the vector table to _vtor_base (the GD32 CMSIS uses a
 * VECT_TAB_OFFSET; set it so SCB->VTOR = 0x08008000). The bootloader also sets
 * VTOR before jumping, so either path is safe.
 */

ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08008000, LENGTH = 238K   /* 240K slot - 2K manifest */
    SRAM  (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
}

_vtor_base      = 0x08008000;
_estack         = ORIGIN(SRAM) + LENGTH(SRAM);
_Min_Heap_Size  = 0x800;
_Min_Stack_Size = 0x1000;

INCLUDE papr_sections.ld
