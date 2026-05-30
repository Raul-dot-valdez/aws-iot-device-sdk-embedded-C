/* GNU linker script — PAPR application in SLOT B (GD32E517RE).
 *
 *   slot B app  : 0x08044000  238 KB
 *   manifest    : 0x0807F800    2 KB  (signed trailer, written by OTA commit)
 *
 * Identical to app_slot_a.ld but linked for the B slot. In a direct-XIP dual
 * bank scheme the signing/release flow produces both an A image and a B image;
 * the OTA server hands the device whichever targets its inactive slot.
 */

ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08044000, LENGTH = 238K   /* 240K slot - 2K manifest */
    SRAM  (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
}

_vtor_base      = 0x08044000;
_estack         = ORIGIN(SRAM) + LENGTH(SRAM);
_Min_Heap_Size  = 0x800;
_Min_Stack_Size = 0x1000;

INCLUDE papr_sections.ld
