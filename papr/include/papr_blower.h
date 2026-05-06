#ifndef PAPR_BLOWER_H
#define PAPR_BLOWER_H

#include "papr_types.h"

typedef struct
{
    int32_t kp_q16;
    int32_t ki_q16;
    int32_t kd_q16;
    int32_t integral;
    int32_t prev_error;
    uint16_t duty;
    uint16_t setpoint_lpm;
    bool     saturated;
} papr_blower_t;

papr_status_t papr_blower_init(papr_blower_t *b);
papr_status_t papr_blower_start(papr_blower_t *b);
papr_status_t papr_blower_stop(papr_blower_t *b);

void papr_blower_set_target(papr_blower_t *b, uint16_t lpm);

/* Single PID step. dt_ms must be the elapsed time since the previous call. */
papr_status_t papr_blower_update(papr_blower_t *b,
                                 uint16_t measured_lpm,
                                 uint32_t dt_ms);

uint16_t papr_blower_duty_permille(const papr_blower_t *b);

#endif /* PAPR_BLOWER_H */
