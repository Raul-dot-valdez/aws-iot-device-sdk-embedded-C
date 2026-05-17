#ifndef PAPR_CONTROLLER_H
#define PAPR_CONTROLLER_H

#include "papr_alarms.h"
#include "papr_battery.h"
#include "papr_ble.h"
#include "papr_blower.h"
#include "papr_energy.h"
#include "papr_keypad.h"
#include "papr_sensors.h"
#include "papr_types.h"

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

#endif /* PAPR_CONTROLLER_H */
