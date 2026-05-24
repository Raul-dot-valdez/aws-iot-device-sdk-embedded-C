#ifndef PAPR_PROVISION_H
#define PAPR_PROVISION_H

#include "papr_types.h"
#include "papr_version.h"

/* Per-device provisioning record, written once by the production flashing /
 * test station and read by the application at boot. Lives in a dedicated
 * flash page (see ARCHITECTURE.md "OTA + provisioning flash map") that is
 * never touched by an OTA update, so calibration and identity survive field
 * firmware upgrades.
 *
 * Design-for-Manufacture intent:
 *   - unique, human-readable serial for traceability (laser-marked + stored)
 *   - hardware revision so firmware can branch on board spins
 *   - manufacture date for warranty / shelf-life tracking
 *   - the BLE MAC the GD32VW553 module was provisioned with
 *   - per-unit calibration so identical firmware runs on units that differ
 *     within component tolerance (flow sensor gain/offset, shunt offset)
 *
 * The struct is kept small and explicitly padded so its on-flash layout is
 * stable across compilers; a CRC-32 guards against a half-written page. */

#define PAPR_PROV_MAGIC        0x50525056U   /* 'P''R''P''V' little-endian */
#define PAPR_PROV_STRUCT_VER   1U
#define PAPR_PROV_SERIAL_LEN   16U

typedef struct
{
    uint32_t          magic;                 /* PAPR_PROV_MAGIC            */
    uint16_t          struct_ver;            /* PAPR_PROV_STRUCT_VER       */
    uint16_t          hw_rev;                 /* board revision, BCD eg 0x0102 = rev1.2 */
    char              serial[PAPR_PROV_SERIAL_LEN]; /* NUL-padded ASCII    */
    uint32_t          mfg_date;              /* YYYYMMDD decimal           */
    uint8_t           ble_mac[6];
    uint8_t           reserved0[2];
    int16_t           flow_offset_lpm;       /* added to raw flow reading  */
    int16_t           flow_gain_q8;          /* Q8.8 multiplier, 256 = 1.0 */
    int16_t           ibat_offset_ma;        /* shunt-amp zero offset      */
    int16_t           reserved1;
    uint32_t          crc32;                 /* over all preceding bytes   */
} papr_provision_t;

/* Loads and validates the provisioning page from flash into *out. Returns
 * PAPR_OK only if the magic, struct version, and CRC all check out. */
papr_status_t papr_provision_load(papr_provision_t *out);

/* True if the record is structurally valid (magic + version + CRC). */
bool papr_provision_valid(const papr_provision_t *p);

/* Computes the CRC field for a fully-populated record (station helper) and
 * writes the whole record to the flash provisioning page via the HAL. */
papr_status_t papr_provision_save(papr_provision_t *p);

/* Calibration application helpers. The application's sensor path can call
 * these so identical firmware produces correct readings on each unit. With an
 * unprovisioned / invalid record they fall back to identity (gain 1.0,
 * offset 0), so a blank board still runs. */
uint16_t papr_provision_apply_flow(const papr_provision_t *p, uint16_t raw_lpm);
uint16_t papr_provision_apply_ibat(const papr_provision_t *p, uint16_t raw_ma);

/* Returns a serial string: the provisioned one if valid, otherwise a string
 * derived from the MCU's unique die ID so even a blank unit is identifiable
 * at the test station. out must hold at least PAPR_PROV_SERIAL_LEN+1 bytes. */
void papr_provision_serial(const papr_provision_t *p, char *out);

#endif /* PAPR_PROVISION_H */
