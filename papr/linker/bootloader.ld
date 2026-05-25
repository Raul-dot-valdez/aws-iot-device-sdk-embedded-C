/* GNU linker script — PAPR reference BOOTLOADER (GD32E517RE).
 *
 * Occupies the first 28 KB of flash. The boot-state page (0x08007000) and the
 * provisioning page (0x08007800) follow it and are NOT part of this image —
 * the bootloader writes them at runtime via the FMC. See boot/boot_shared.h.
 *
 *   bootloader code : 0x08000000  28 KB
 *   boot-state page : 0x08007000   2 KB  (runtime-written)
 *   provisioning    : 0x08007800   2 KB  (runtime-written)
 */

ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 28K
    SRAM  (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
}

_vtor_base      = 0x08000000;
_estack         = ORIGIN(SRAM) + LENGTH(SRAM);
_Min_Heap_Size  = 0x400;
_Min_Stack_Size = 0x1000;

INCLUDE papr_sections.ld
