#ifndef PAPR_CONTROLLER_H
#define PAPR_CONTROLLER_H

#include "papr_alarms.h"
#include "papr_battery.h"
#include "papr_blower.h"
#include "papr_sensors.h"
#include "papr_types.h"

typedef struct
{
    papr_state_t        state;
    papr_flow_level_t   level;
    papr_blower_t       blower;
    papr_battery_t      battery;
    papr_sensors_t      sensors;
    papr_alarms_t       alarms;
    uint32_t            last_control_ms;
    uint32_t            last_sensor_ms;
    uint32_t            last_wdt_ms;
    uint32_t            runtime_seconds;
    uint32_t            state_entered_ms;
} papr_controller_t;

papr_status_t papr_controller_init(papr_controller_t *c);

/* Runs one iteration of the supervisor. Call as fast as the scheduler allows;
 * the controller paces itself using the configured periods. */
papr_status_t papr_controller_step(papr_controller_t *c);

void papr_controller_request_shutdown(papr_controller_t *c);
void papr_controller_cycle_level(papr_controller_t *c);

void papr_controller_get_telemetry(const papr_controller_t *c,
                                   papr_telemetry_t *out);

#endif /* PAPR_CONTROLLER_H */
