#ifndef PAPR_VERSION_H
#define PAPR_VERSION_H

#include <stdint.h>

/* Single source of truth for the firmware version. Reported over BLE
 * (GET_VERSION, OTA_STATUS) so the mobile app can tell the user whether the
 * device is up to date, and checked by the OTA module to reject downgrades.
 *
 * Bump on every released revision. History:
 *   0.1.0  rev 1  core controller + blower
 *   0.2.0  rev 2  L6235 driver
 *   0.3.0  rev 3  GD32E517RE port
 *   0.4.0  rev 4  SDP810 differential pressure
 *   0.5.0  rev 5  GDY1124 + BLE module
 *   0.6.0  rev 6  adaptive comfort + switch matrix
 *   0.7.0  rev 7  OTA firmware update
 */
#define PAPR_FW_VERSION_MAJOR   0U
#define PAPR_FW_VERSION_MINOR   7U
#define PAPR_FW_VERSION_PATCH   0U

typedef struct
{
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
} papr_fw_version_t;

static inline papr_fw_version_t papr_version_current(void)
{
    papr_fw_version_t v = {
        PAPR_FW_VERSION_MAJOR,
        PAPR_FW_VERSION_MINOR,
        PAPR_FW_VERSION_PATCH
    };
    return v;
}

/* Returns >0 if a is newer than b, 0 if equal, <0 if a is older. */
static inline int papr_version_cmp(papr_fw_version_t a, papr_fw_version_t b)
{
    if (a.major != b.major) { return (a.major > b.major) ? 1 : -1; }
    if (a.minor != b.minor) { return (a.minor > b.minor) ? 1 : -1; }
    if (a.patch != b.patch) { return (a.patch > b.patch) ? 1 : -1; }
    return 0;
}

#endif /* PAPR_VERSION_H */
