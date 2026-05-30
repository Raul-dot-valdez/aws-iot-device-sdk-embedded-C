#include "papr_blower.h"
#include "papr_config.h"

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
    b->current_ma    = 0U;
    b->setpoint_lpm  = PAPR_FLOW_LOW_LPM;
    b->saturated     = false;
    return papr_l6235_init(&b->driver);
}

papr_status_t papr_blower_start(papr_blower_t *b)
{
    if (b == NULL) { return PAPR_ERR_PARAM; }
    b->integral   = 0;
    b->prev_error = 0;
    b->current_ma = 0U;
    return papr_l6235_start(&b->driver);
}

papr_status_t papr_blower_stop(papr_blower_t *b)
{
    if (b == NULL) { return PAPR_ERR_PARAM; }
    b->current_ma = 0U;
    return papr_l6235_stop(&b->driver);
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

    (void)papr_l6235_poll(&b->driver);
    if (papr_l6235_has_fault(&b->driver))
    {
        b->current_ma = 0U;
        return PAPR_ERR_HW;
    }

    int32_t error  = (int32_t)b->setpoint_lpm - (int32_t)measured_lpm;
    int32_t dt     = (int32_t)dt_ms;

    /* Anti-windup: skip integration when the actuator is saturated and the
     * error pushes further into saturation. */
    bool clamp_high = b->saturated &&
                      (b->current_ma >= PAPR_L6235_IMAX_MA) && (error > 0);
    bool clamp_low  = b->saturated &&
                      (b->current_ma == 0U) && (error < 0);
    if (!clamp_high && !clamp_low)
    {
        b->integral += error * dt;
        b->integral  = clamp_i32(b->integral, -1000000, 1000000);
    }

    int32_t derivative = ((error - b->prev_error) * 1000) / dt;
    b->prev_error      = error;

    int64_t out_q16 = (int64_t)b->kp_q16 * error
                    + ((int64_t)b->ki_q16 * b->integral) / 1000
                    + ((int64_t)b->kd_q16 * derivative)  / 1000;

    int32_t cmd_ma = (int32_t)(out_q16 >> 16);
    cmd_ma         = clamp_i32(cmd_ma, 0, (int32_t)PAPR_L6235_IMAX_MA);
    b->saturated   = (cmd_ma == 0 || cmd_ma == (int32_t)PAPR_L6235_IMAX_MA);
    b->current_ma  = (uint16_t)cmd_ma;

    return papr_l6235_set_current_ma(&b->driver, b->current_ma);
}

uint16_t papr_blower_duty_permille(const papr_blower_t *b)
{
    return (b == NULL) ? 0U : papr_l6235_demand_permille(&b->driver);
}

uint16_t papr_blower_rpm(const papr_blower_t *b)
{
    return (b == NULL) ? 0U : papr_l6235_rpm(&b->driver);
}

bool papr_blower_driver_fault(const papr_blower_t *b)
{
    return (b != NULL) && papr_l6235_has_fault(&b->driver);
}
