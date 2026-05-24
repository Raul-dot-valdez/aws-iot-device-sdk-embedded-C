#ifndef PAPR_FACTORY_H
#define PAPR_FACTORY_H

#include "papr_types.h"

/* Production (end-of-line) test support.
 *
 * Entered from main() when papr_hal_factory_requested() is true at boot
 * (the test fixture asserts the TEST_MODE pad). Instead of the normal
 * supervisor, the firmware runs a command loop spoken over the same UART the
 * BLE module uses — the fixture taps the UART test pads. It uses the same
 * framing as the BLE link (AA 55 LEN CMD PAYLOAD CRC8) so one parser and one
 * CRC serve both, easing fixture software reuse.
 *
 * Capabilities exposed to the station:
 *   - identify the unit (firmware version, serial, hw rev, provisioned?)
 *   - run a built-in self-test (BIST) over every subsystem, returning a
 *     pass/fail mask plus the measured values for the station's log
 *   - read / write the provisioning page (serial, calibration, BLE MAC)
 *   - manually actuate outputs (LED, buzzer, blower) for fixture sensors
 *   - read raw sensor values
 *   - reboot into the application once the unit passes
 */

/* BIST subsystem bits (executed_mask / pass_mask). */
typedef enum
{
    PAPR_BIST_PROVISION = 0x0001U,
    PAPR_BIST_BATTERY   = 0x0002U,
    PAPR_BIST_TEMP      = 0x0004U,
    PAPR_BIST_SDP810    = 0x0008U,
    PAPR_BIST_GDY1124   = 0x0010U,
    PAPR_BIST_BLOWER    = 0x0020U,   /* spins motor, checks RPM + current   */
    PAPR_BIST_TACHO     = 0x0040U,
    PAPR_BIST_FLOW      = 0x0080U,   /* flow rises when blower runs          */
    PAPR_BIST_KEYPAD    = 0x0100U,   /* no stuck keys                        */
    PAPR_BIST_LED       = 0x0200U,   /* actuated (fixture verifies optically)*/
    PAPR_BIST_BUZZER    = 0x0400U,   /* actuated (fixture verifies acoustic) */
    PAPR_BIST_BLE       = 0x0800U    /* module reset pulse issued            */
} papr_bist_bit_t;

typedef struct
{
    uint16_t executed_mask;          /* which subsystems were tested        */
    uint16_t pass_mask;              /* which passed                        */
    uint16_t blower_rpm;
    uint16_t blower_current_ma;
    uint16_t flow_lpm;
    int16_t  sdp_pressure_pa;
    uint32_t baro_pressure_pa;
    uint16_t battery_mv;
    int16_t  temperature_c10;
} papr_bist_report_t;

/* Runs the full self-test and fills *report. Returns true if every executed
 * subsystem passed (executed_mask == pass_mask). Safe to call standalone for
 * a host-side smoke test. */
bool papr_factory_run_bist(papr_bist_report_t *report);

/* Enters the factory command loop. Assumes papr_hal_init() has run. Does not
 * return on real hardware (a FCT_REBOOT resets into the application); on the
 * host it returns when the station sends FCT_REBOOT so tests can continue. */
void papr_factory_main(void);

#endif /* PAPR_FACTORY_H */
