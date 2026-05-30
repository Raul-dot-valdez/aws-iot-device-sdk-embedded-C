#ifndef PAPR_ENERGY_H
#define PAPR_ENERGY_H

#include "papr_config.h"
#include "papr_types.h"

/* Adaptive comfort / battery-saving module.
 *
 * Three responsibilities:
 *
 * 1. Breathing pattern detection — watches the SDP810 differential-pressure
 *    stream and tracks (a) instantaneous breaths-per-minute and (b) a moving
 *    estimate of breath intensity (peak-to-peak amplitude in Pa).
 *
 * 2. Auto level selection — when auto mode is enabled, decides whether to
 *    step the blower flow setpoint up or down based on breathing rate /
 *    intensity. A hysteresis hold-off prevents level hunting; the floor is
 *    always PAPR_LEVEL_LOW so the user is never deprived of certified flow.
 *
 * 3. Battery runtime estimation — integrates pack current into a coulomb
 *    counter, converts state-of-charge to mAh, and reports an estimate of
 *    minutes remaining at the current draw. Per-shift session lengths are
 *    smoothed with an EWMA so the energy budget adapts to the individual
 *    worker's typical pattern across power cycles. */

typedef struct
{
    /* --- Breathing detection ---------------------------------------- */
    int16_t  dp_window[PAPR_ENERGY_WINDOW_SAMPLES];
    uint8_t  dp_head;
    int32_t  dp_sum;                  /* running sum for moving average  */
    int16_t  dp_last_filtered;
    int8_t   dp_direction;            /* -1 falling, +1 rising            */
    uint32_t last_zero_cross_ms;
    uint8_t  breaths_per_min;
    uint16_t breath_amplitude_pa;

    /* --- Auto level decisioning ------------------------------------- */
    bool                auto_enabled;
    papr_flow_level_t   suggested_level;
    uint32_t            last_level_change_ms;

    /* --- Battery / runtime ------------------------------------------ */
    uint32_t coulomb_mas;             /* accumulated milliamp-seconds    */
    uint16_t remaining_minutes;
    uint32_t last_sample_ms;

    /* --- Session tracking ------------------------------------------- */
    uint32_t session_start_ms;
    uint16_t typical_session_minutes; /* EWMA across the device's life    */
    bool     in_session;
} papr_energy_t;

papr_status_t papr_energy_init(papr_energy_t *e);

void papr_energy_set_auto(papr_energy_t *e, bool enabled);
bool papr_energy_get_auto(const papr_energy_t *e);

/* Called every supervisor tick. dP comes straight from the SDP810; the rest
 * are the live telemetry fields used to update the runtime estimator. */
void papr_energy_update(papr_energy_t *e,
                        int16_t dp_pa,
                        uint16_t battery_ma,
                        uint8_t soc_percent,
                        bool blower_running,
                        uint32_t now_ms);

/* Returns the level the energy module currently recommends. When auto mode
 * is disabled this is the most recent level it has seen the user select. */
papr_flow_level_t papr_energy_suggest_level(const papr_energy_t *e,
                                            papr_flow_level_t current);

uint8_t  papr_energy_breaths_per_min(const papr_energy_t *e);
uint16_t papr_energy_remaining_minutes(const papr_energy_t *e);
uint16_t papr_energy_typical_session_min(const papr_energy_t *e);

/* Called by the supervisor on state transitions so the module can mark the
 * start / end of a session and update the EWMA. */
void papr_energy_on_running(papr_energy_t *e, uint32_t now_ms);
void papr_energy_on_idle(papr_energy_t *e, uint32_t now_ms);

#endif /* PAPR_ENERGY_H */
