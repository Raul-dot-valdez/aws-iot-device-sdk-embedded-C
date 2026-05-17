#include "papr_energy.h"

#include <string.h>

/* Absolute-value helper for int16, avoiding the abs() macro variants. */
static uint16_t abs_i16(int16_t v) { return (v < 0) ? (uint16_t)-v : (uint16_t)v; }

papr_status_t papr_energy_init(papr_energy_t *e)
{
    if (e == NULL) { return PAPR_ERR_PARAM; }
    memset(e, 0, sizeof(*e));
    e->dp_direction            = 1;
    e->auto_enabled            = false;
    e->suggested_level         = PAPR_LEVEL_MED;
    e->typical_session_minutes = 60U;   /* sensible 1-hour default */
    return PAPR_OK;
}

void papr_energy_set_auto(papr_energy_t *e, bool enabled)
{
    if (e != NULL) { e->auto_enabled = enabled; }
}

bool papr_energy_get_auto(const papr_energy_t *e)
{
    return (e != NULL) && e->auto_enabled;
}

/* --- Breathing detection ---------------------------------------------------
 * Maintain a moving average to estimate the DC level of dP, then detect
 * threshold-crossings of (dp - moving_avg). Each rising threshold-crossing
 * counts as one inhale onset; the period between successive onsets gives
 * breaths-per-minute. Amplitude is the peak excursion within the window. */

static void breathing_step(papr_energy_t *e, int16_t dp_pa, uint32_t now_ms)
{
    /* Slide the dp_window ring buffer; maintain dp_sum incrementally. */
    int16_t evicted = e->dp_window[e->dp_head];
    e->dp_sum    -= evicted;
    e->dp_sum    += dp_pa;
    e->dp_window[e->dp_head] = dp_pa;
    e->dp_head    = (uint8_t)((e->dp_head + 1U) % PAPR_ENERGY_WINDOW_SAMPLES);

    int16_t mean      = (int16_t)(e->dp_sum / (int32_t)PAPR_ENERGY_WINDOW_SAMPLES);
    int16_t centered  = (int16_t)((int32_t)dp_pa - mean);

    /* Threshold-crossing detector with hysteresis. The directional state
     * machine fires once per breath when the signal transitions from below
     * the negative threshold to above the positive one. */
    int16_t  thr  = (int16_t)PAPR_ENERGY_BREATH_THRESHOLD_PA;
    bool fired = false;
    if (e->dp_direction <= 0 && centered > thr)
    {
        e->dp_direction = 1;
        fired = true;
    }
    else if (e->dp_direction >= 0 && centered < -thr)
    {
        e->dp_direction = -1;
    }

    if (fired)
    {
        uint32_t period_ms = (uint32_t)(now_ms - e->last_zero_cross_ms);
        e->last_zero_cross_ms = now_ms;
        if (period_ms >= 1500U && period_ms <= 10000U)
        {
            uint32_t bpm = 60000U / period_ms;
            /* EWMA to keep BPM jitter-free. */
            e->breaths_per_min = (uint8_t)(((uint32_t)e->breaths_per_min * 3U + bpm) / 4U);
        }
        else
        {
            /* Out-of-range gap → decay toward zero, not noise. */
            e->breaths_per_min = (uint8_t)((uint32_t)e->breaths_per_min * 3U / 4U);
        }
    }

    /* Amplitude tracker — peak |centered| within the window. */
    uint16_t cur_amp = abs_i16(centered);
    if (cur_amp > e->breath_amplitude_pa)
    {
        e->breath_amplitude_pa = cur_amp;
    }
    else
    {
        /* Slow leak so the metric stays current after a deep breath. */
        e->breath_amplitude_pa = (uint16_t)((uint32_t)e->breath_amplitude_pa * 31U / 32U);
    }
    e->dp_last_filtered = centered;
}

/* --- Auto level decisioning ------------------------------------------------ */

static void auto_level_step(papr_energy_t *e, uint32_t now_ms)
{
    if (!e->auto_enabled) { return; }
    if ((uint32_t)(now_ms - e->last_level_change_ms) < PAPR_ENERGY_LEVEL_HOLD_MS)
    {
        return;
    }

    papr_flow_level_t target = e->suggested_level;
    if (e->breaths_per_min >= PAPR_ENERGY_BPM_HEAVY ||
        e->breath_amplitude_pa > 80U)
    {
        if (target < PAPR_LEVEL_HIGH) { target = (papr_flow_level_t)(target + 1U); }
    }
    else if (e->breaths_per_min != 0U &&
             e->breaths_per_min <= PAPR_ENERGY_BPM_LIGHT &&
             e->breath_amplitude_pa < 40U)
    {
        if (target > PAPR_LEVEL_LOW) { target = (papr_flow_level_t)(target - 1U); }
    }

    if (target != e->suggested_level)
    {
        e->suggested_level     = target;
        e->last_level_change_ms = now_ms;
    }
}

/* --- Battery runtime estimation ------------------------------------------- */

static void runtime_step(papr_energy_t *e,
                         uint16_t battery_ma,
                         uint8_t soc_percent,
                         uint32_t now_ms)
{
    if (e->last_sample_ms != 0U)
    {
        uint32_t dt_ms = (uint32_t)(now_ms - e->last_sample_ms);
        e->coulomb_mas += ((uint32_t)battery_ma * dt_ms) / 1000U;
    }
    e->last_sample_ms = now_ms;

    uint32_t remaining_mah =
        ((uint32_t)PAPR_ENERGY_PACK_MAH * (uint32_t)soc_percent) / 100U;

    /* Minutes = remaining_mAh / mA * 60. Guard against zero current. */
    uint16_t minutes = 0U;
    if (battery_ma >= 50U)
    {
        uint32_t m = (remaining_mah * 60U) / battery_ma;
        if (m > 0xFFFFU) { m = 0xFFFFU; }
        minutes = (uint16_t)m;
    }
    else
    {
        minutes = (uint16_t)(remaining_mah / 5U); /* idle-ish fallback */
    }
    e->remaining_minutes = minutes;
}

void papr_energy_update(papr_energy_t *e,
                        int16_t dp_pa,
                        uint16_t battery_ma,
                        uint8_t soc_percent,
                        bool blower_running,
                        uint32_t now_ms)
{
    if (e == NULL) { return; }

    if (blower_running)
    {
        breathing_step(e, dp_pa, now_ms);
        auto_level_step(e, now_ms);
    }
    runtime_step(e, battery_ma, soc_percent, now_ms);
}

papr_flow_level_t papr_energy_suggest_level(const papr_energy_t *e,
                                            papr_flow_level_t current)
{
    if (e == NULL) { return current; }
    return e->auto_enabled ? e->suggested_level : current;
}

uint8_t papr_energy_breaths_per_min(const papr_energy_t *e)
{
    return (e == NULL) ? 0U : e->breaths_per_min;
}

uint16_t papr_energy_remaining_minutes(const papr_energy_t *e)
{
    return (e == NULL) ? 0U : e->remaining_minutes;
}

uint16_t papr_energy_typical_session_min(const papr_energy_t *e)
{
    return (e == NULL) ? 0U : e->typical_session_minutes;
}

void papr_energy_on_running(papr_energy_t *e, uint32_t now_ms)
{
    if (e == NULL || e->in_session) { return; }
    e->in_session       = true;
    e->session_start_ms = now_ms;
}

void papr_energy_on_idle(papr_energy_t *e, uint32_t now_ms)
{
    if (e == NULL || !e->in_session) { return; }
    uint32_t duration_min = (uint32_t)((now_ms - e->session_start_ms) / 60000U);
    e->in_session = false;

    if (duration_min < 1U) { return; }   /* ignore taps shorter than a minute */
    /* EWMA: new = ((256-alpha)*old + alpha*sample) / 256 */
    uint32_t alpha   = PAPR_ENERGY_SESSION_EWMA_NUM;
    uint32_t blended =
        ((256U - alpha) * (uint32_t)e->typical_session_minutes +
         alpha * duration_min) / 256U;
    if (blended > 0xFFFFU) { blended = 0xFFFFU; }
    e->typical_session_minutes = (uint16_t)blended;
}
