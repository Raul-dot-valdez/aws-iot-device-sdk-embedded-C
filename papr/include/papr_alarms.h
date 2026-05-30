#ifndef PAPR_ALARMS_H
#define PAPR_ALARMS_H

#include "papr_types.h"

typedef struct
{
    uint32_t low_flow_since_ms;
    uint32_t filter_clog_since_ms;
    uint32_t active_mask;       /* OR of papr_alarm_t */
    uint32_t latched_mask;
    bool     muted;
    uint32_t mute_until_ms;
} papr_alarms_t;

void papr_alarms_init(papr_alarms_t *a);

void papr_alarms_set(papr_alarms_t *a, papr_alarm_t alarm);
void papr_alarms_clear(papr_alarms_t *a, papr_alarm_t alarm);

/* Latches alarms based on the time history. The caller passes the relevant
 * measurements; the function updates active/latched masks accordingly. */
void papr_alarms_evaluate(papr_alarms_t *a,
                          uint16_t flow_lpm,
                          uint16_t duty_permille,
                          uint32_t now_ms);

void papr_alarms_mute(papr_alarms_t *a, uint32_t now_ms, uint32_t duration_ms);

bool papr_alarms_is_critical(const papr_alarms_t *a);

/* Drives the buzzer / LED according to the active mask. */
void papr_alarms_render(const papr_alarms_t *a, uint32_t now_ms);

#endif /* PAPR_ALARMS_H */
