#ifndef PAPR_BLOWER_H
#define PAPR_BLOWER_H

#include "papr_l6235.h"
#include "papr_types.h"

/* Closed-loop airflow controller. The PID acts on litres-per-minute error and
 * produces a motor-current setpoint, which is delivered to the L6235 driver
 * via the VREF analog input. The L6235 enforces the current internally
 * through constant-tOFF chopping, so this module never touches PWM directly. */

typedef struct
{
    int32_t kp_q16;
    int32_t ki_q16;
    int32_t kd_q16;
    int32_t integral;
    int32_t prev_error;
    uint16_t current_ma;        /* Last commanded current */
    uint16_t setpoint_lpm;
    bool     saturated;
    papr_l6235_t driver;
} papr_blower_t;

papr_status_t papr_blower_init(papr_blower_t *b);
papr_status_t papr_blower_start(papr_blower_t *b);
papr_status_t papr_blower_stop(papr_blower_t *b);

void papr_blower_set_target(papr_blower_t *b, uint16_t lpm);

/* Single PID step. dt_ms must be the elapsed time since the previous call. */
papr_status_t papr_blower_update(papr_blower_t *b,
                                 uint16_t measured_lpm,
                                 uint32_t dt_ms);

/* Demand reported as 0..1000 ‰ of the L6235 current ceiling. Used by the
 * alarm subsystem (filter-clog detection) and by the telemetry path. */
uint16_t papr_blower_duty_permille(const papr_blower_t *b);

uint16_t papr_blower_rpm(const papr_blower_t *b);
bool     papr_blower_driver_fault(const papr_blower_t *b);

#endif /* PAPR_BLOWER_H */
