#include "papr_blower.h"
#include "papr_config.h"
#include "papr_hal.h"

static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

papr_status_t papr_blower_init(papr_blower_t *b)
{
    if (b == NULL) { return PAPR_ERR_PARAM; }
    b->kp_q16        = PAPR_PID_KP_Q16;
    b->ki_q16        = PAPR_PID_KI_Q16;
    b->kd_q16        = PAPR_PID_KD_Q16;
    b->integral      = 0;
    b->prev_error    = 0;
    b->duty          = 0;
    b->setpoint_lpm  = PAPR_FLOW_LOW_LPM;
    b->saturated     = false;
    return papr_hal_blower_set_duty(0);
}

papr_status_t papr_blower_start(papr_blower_t *b)
{
    if (b == NULL) { return PAPR_ERR_PARAM; }
    b->integral   = 0;
    b->prev_error = 0;
    b->duty       = 0;
    return papr_hal_blower_enable(true);
}

papr_status_t papr_blower_stop(papr_blower_t *b)
{
    if (b == NULL) { return PAPR_ERR_PARAM; }
    b->duty = 0;
    (void)papr_hal_blower_set_duty(0);
    return papr_hal_blower_enable(false);
}

void papr_blower_set_target(papr_blower_t *b, uint16_t lpm)
{
    if (b != NULL) { b->setpoint_lpm = lpm; }
}

papr_status_t papr_blower_update(papr_blower_t *b,
                                 uint16_t measured_lpm,
                                 uint32_t dt_ms)
{
    if (b == NULL || dt_ms == 0U) { return PAPR_ERR_PARAM; }

    int32_t error  = (int32_t)b->setpoint_lpm - (int32_t)measured_lpm;
    int32_t dt     = (int32_t)dt_ms;

    /* Anti-windup: skip integration when the actuator is saturated and the
     * error pushes further into saturation. */
    if (!(b->saturated &&
          ((b->duty >= PAPR_PWM_MAX && error > 0) ||
           (b->duty == 0U          && error < 0))))
    {
        b->integral += error * dt;
        b->integral  = clamp_i32(b->integral, -1000000, 1000000);
    }

    int32_t derivative = ((error - b->prev_error) * 1000) / dt;
    b->prev_error      = error;

    int64_t out_q16 = (int64_t)b->kp_q16 * error
                    + ((int64_t)b->ki_q16 * b->integral) / 1000
                    + ((int64_t)b->kd_q16 * derivative)  / 1000;

    int32_t duty = (int32_t)(out_q16 >> 16);
    duty         = clamp_i32(duty, 0, (int32_t)PAPR_PWM_MAX);
    b->saturated = (duty == 0 || duty == (int32_t)PAPR_PWM_MAX);
    b->duty      = (uint16_t)duty;

    return papr_hal_blower_set_duty(b->duty);
}

uint16_t papr_blower_duty_permille(const papr_blower_t *b)
{
    if (b == NULL || PAPR_PWM_MAX == 0U) { return 0U; }
    uint32_t scaled = (uint32_t)b->duty * 1000U / PAPR_PWM_MAX;
    return (uint16_t)scaled;
}
