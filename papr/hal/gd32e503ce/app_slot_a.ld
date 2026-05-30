/* GNU linker script — PAPR application in SLOT A (GD32E503CE, 256 KB flash).
 *
 *   slot A app  : 0x08008000  111 KB
 *   manifest    : 0x08023C00    1 KB  (signed trailer, written by OTA commit)
 */

ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08008000, LENGTH = 111K   /* 112K slot - 1K manifest */
    SRAM  (xrw) : ORIGIN = 0x20000000, LENGTH = 64K
}

_vtor_base      = 0x08008000;
_estack         = ORIGIN(SRAM) + LENGTH(SRAM);
_Min_Heap_Size  = 0x400;
_Min_Stack_Size = 0x1000;

INCLUDE papr_sections.ld
