#include "papr_alarms.h"
#include "papr_config.h"
#include "papr_hal.h"

#define LED_OK      0U
#define LED_WARN    1U
#define LED_FAULT   2U

static uint32_t elapsed_ms(uint32_t now, uint32_t since)
{
    return (uint32_t)(now - since);
}

void papr_alarms_init(papr_alarms_t *a)
{
    if (a == NULL) { return; }
    a->low_flow_since_ms     = 0U;
    a->filter_clog_since_ms  = 0U;
    a->active_mask           = 0U;
    a->latched_mask          = 0U;
    a->muted                 = false;
    a->mute_until_ms         = 0U;
}

void papr_alarms_set(papr_alarms_t *a, papr_alarm_t alarm)
{
    if (a == NULL) { return; }
    a->active_mask  |= (uint32_t)alarm;
    a->latched_mask |= (uint32_t)alarm;
}

void papr_alarms_clear(papr_alarms_t *a, papr_alarm_t alarm)
{
    if (a == NULL) { return; }
    a->active_mask &= ~(uint32_t)alarm;
}

void papr_alarms_evaluate(papr_alarms_t *a,
                          uint16_t flow_lpm,
                          uint16_t duty_permille,
                          uint32_t now_ms)
{
    if (a == NULL) { return; }

    if (flow_lpm < PAPR_FLOW_ALARM_LPM)
    {
        if (a->low_flow_since_ms == 0U) { a->low_flow_since_ms = now_ms; }
        if (elapsed_ms(now_ms, a->low_flow_since_ms) >= PAPR_FLOW_ALARM_DEBOUNCE_MS)
        {
            papr_alarms_set(a, PAPR_ALARM_LOW_FLOW);
        }
    }
    else
    {
        a->low_flow_since_ms = 0U;
        papr_alarms_clear(a, PAPR_ALARM_LOW_FLOW);
    }

    if (duty_permille >= PAPR_FILTER_CLOG_DUTY_PERMILLE)
    {
        if (a->filter_clog_since_ms == 0U) { a->filter_clog_since_ms = now_ms; }
        if (elapsed_ms(now_ms, a->filter_clog_since_ms) >= PAPR_FILTER_CLOG_DEBOUNCE_MS)
        {
            papr_alarms_set(a, PAPR_ALARM_FILTER_CLOG);
        }
    }
    else
    {
        a->filter_clog_since_ms = 0U;
        papr_alarms_clear(a, PAPR_ALARM_FILTER_CLOG);
    }

    if (a->muted && (int32_t)(now_ms - a->mute_until_ms) >= 0)
    {
        a->muted = false;
    }
}

void papr_alarms_mute(papr_alarms_t *a, uint32_t now_ms, uint32_t duration_ms)
{
    if (a == NULL) { return; }
    a->muted         = true;
    a->mute_until_ms = now_ms + duration_ms;
}

bool papr_alarms_is_critical(const papr_alarms_t *a)
{
    if (a == NULL) { return false; }
    const uint32_t critical = (uint32_t)PAPR_ALARM_CRIT_BATTERY |
                              (uint32_t)PAPR_ALARM_MOTOR_FAULT  |
                              (uint32_t)PAPR_ALARM_OVERTEMP     |
                              (uint32_t)PAPR_ALARM_SENSOR_FAULT;
    return (a->active_mask & critical) != 0U;
}

void papr_alarms_render(const papr_alarms_t *a, uint32_t now_ms)
{
    if (a == NULL) { return; }

    bool fault = papr_alarms_is_critical(a);
    bool warn  = (a->active_mask &
                  ((uint32_t)PAPR_ALARM_LOW_FLOW    |
                   (uint32_t)PAPR_ALARM_LOW_BATTERY |
                   (uint32_t)PAPR_ALARM_FILTER_CLOG)) != 0U;

    (void)papr_hal_led_set(LED_OK,    !(fault || warn));
    (void)papr_hal_led_set(LED_WARN,  warn && !fault);
    (void)papr_hal_led_set(LED_FAULT, fault);

    if (a->active_mask == 0U || a->muted)
    {
        (void)papr_hal_buzzer_set(false, 0U);
        return;
    }

    /* Critical: continuous 4 kHz tone. Warning: 2 kHz, 50 % duty 1 Hz cadence. */
    if (fault)
    {
        (void)papr_hal_buzzer_set(true, 4000U);
    }
    else
    {
        bool on = ((now_ms / 500U) & 1U) == 0U;
        (void)papr_hal_buzzer_set(on, 2000U);
    }
}
