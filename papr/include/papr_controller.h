#ifndef PAPR_CONTROLLER_H
#define PAPR_CONTROLLER_H

#include "papr_alarms.h"
#include "papr_battery.h"
#include "papr_ble.h"
#include "papr_blower.h"
#include "papr_energy.h"
#include "papr_keypad.h"
#include "papr_ota.h"
#include "papr_secure.h"
#include "papr_sensors.h"
#include "papr_types.h"
#include "papr_version.h"

typedef struct papr_controller
{
    papr_state_t        state;
    papr_flow_level_t   level;
    papr_blower_t       blower;
    papr_battery_t      battery;
    papr_sensors_t      sensors;
    papr_alarms_t       alarms;
    papr_ble_t          ble;
    papr_keypad_t       keypad;
    papr_energy_t       energy;
    papr_secure_t       secure;
    papr_ota_t          ota;
    bool                boot_confirmed;   /* OTA rollback confirmation done */
    uint32_t            last_control_ms;
    uint32_t            last_sensor_ms;
    uint32_t            last_wdt_ms;
    uint32_t            runtime_seconds;
    uint32_t            state_entered_ms;

    /* Latched requests from the BLE remote-control path. The supervisor
     * picks them up at the next tick rather than touching state directly
     * from a UART callback. */
    bool                rc_power_on_pending;
    bool                rc_power_off_pending;
    bool                rc_level_pending;
    papr_flow_level_t   rc_level_request;
    bool                rc_reset_fault_pending;
    uint32_t            rc_mute_pending_ms;     /* 0 = no request */
} papr_controller_t;

papr_status_t papr_controller_init(papr_controller_t *c);

/* Runs one iteration of the supervisor. Call as fast as the scheduler allows;
 * the controller paces itself using the configured periods. */
papr_status_t papr_controller_step(papr_controller_t *c);

void papr_controller_request_shutdown(papr_controller_t *c);
void papr_controller_cycle_level(papr_controller_t *c);

void papr_controller_get_telemetry(const papr_controller_t *c,
                                   papr_telemetry_t *out);

/* Snapshot used by the BLE notifier: telemetry + state + level + alarm mask
 * in one call so the wireless task does not have to chase fields. */
void papr_controller_snapshot(const papr_controller_t *c,
                              papr_telemetry_t *out_t,
                              uint8_t *out_state,
                              uint8_t *out_level,
                              uint32_t *out_alarms);

/* Remote-control hooks exposed to the BLE module. Each one sets a latched
 * request which the supervisor honours on the next tick. They are safe to
 * call from outside the main loop (UART RX context). */
void papr_controller_remote_power(papr_controller_t *c, bool on);
void papr_controller_remote_set_level(papr_controller_t *c,
                                      papr_flow_level_t level);
void papr_controller_remote_mute(papr_controller_t *c, uint32_t duration_ms);
void papr_controller_remote_reset_fault(papr_controller_t *c);

/* Toggle adaptive-comfort / auto level mode from the remote app. */
void papr_controller_remote_set_auto(papr_controller_t *c, bool enabled);

/* ---- OTA firmware update (driven by the BLE command dispatcher) ----------
 * begin() is gated on the supervisor being idle (STANDBY) so an update can
 * never start while the blower is running. The controller also blocks the
 * blower from starting while an OTA transfer is in progress. */
papr_ota_error_t papr_controller_ota_begin(papr_controller_t *c,
                                           const papr_img_manifest_t *manifest);
papr_ota_error_t papr_controller_ota_write(papr_controller_t *c,
                                           uint32_t offset,
                                           const uint8_t *data, uint16_t len);
papr_ota_error_t papr_controller_ota_finish(papr_controller_t *c);

/* Commits the staged image then resets the MCU. The caller (BLE dispatcher)
 * must flush its acknowledgement before invoking this, since on real hardware
 * it does not return. Returns the error if the image is not READY. */
papr_ota_error_t papr_controller_ota_apply(papr_controller_t *c);
void             papr_controller_ota_abort(papr_controller_t *c);

/* Current OTA state / error / progress for the OTA_STATUS notification. */
void papr_controller_ota_status(const papr_controller_t *c,
                                uint8_t *out_state,
                                uint8_t *out_error,
                                uint8_t *out_percent);

/* ---- Cybersecurity: authenticated control (driven by the BLE dispatcher) -
 * State-changing commands must be gated on papr_controller_control_allowed().
 * The central authenticates with a challenge/response over the session key. */
bool papr_controller_control_allowed(const papr_controller_t *c);
bool papr_controller_auth_begin(papr_controller_t *c,
                                uint8_t nonce_out[PAPR_SEC_NONCE_LEN],
                                uint32_t *counter_out);
bool papr_controller_auth_verify(papr_controller_t *c,
                                 const uint8_t tag[PAPR_SEC_TAG_LEN]);
void papr_controller_session_close(papr_controller_t *c);
void papr_controller_sec_status(const papr_controller_t *c,
                                uint8_t *out_auth_state,
                                uint8_t *out_flags,
                                uint16_t *out_auth_fail,
                                uint16_t *out_ota_reject);

#endif /* PAPR_CONTROLLER_H */
