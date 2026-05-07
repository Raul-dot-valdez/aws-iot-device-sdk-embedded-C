#include "papr_controller.h"
#include "papr_config.h"
#include "papr_hal.h"

static const uint16_t k_flow_setpoints[PAPR_LEVEL_COUNT] = {
    PAPR_FLOW_LOW_LPM,
    PAPR_FLOW_MED_LPM,
    PAPR_FLOW_HIGH_LPM
};

static void enter_state(papr_controller_t *c, papr_state_t s)
{
    c->state            = s;
    c->state_entered_ms = papr_hal_now_ms();
}

static papr_status_t run_self_test(papr_controller_t *c)
{
    papr_status_t s = papr_sensors_update(&c->sensors);
    if (s != PAPR_OK) { return PAPR_ERR_SELF_TEST; }
    s = papr_battery_update(&c->battery);
    if (s != PAPR_OK) { return PAPR_ERR_SELF_TEST; }
    if (c->battery.level == PAPR_BATT_CUTOFF) { return PAPR_ERR_SELF_TEST; }
    return PAPR_OK;
}

static void apply_level(papr_controller_t *c)
{
    if ((unsigned)c->level >= (unsigned)PAPR_LEVEL_COUNT) { c->level = PAPR_LEVEL_LOW; }
    papr_blower_set_target(&c->blower, k_flow_setpoints[c->level]);
}

static void evaluate_safety(papr_controller_t *c, uint32_t now_ms)
{
    /* Battery */
    switch (c->battery.level)
    {
        case PAPR_BATT_LOW:
            papr_alarms_set(&c->alarms, PAPR_ALARM_LOW_BATTERY);
            papr_alarms_clear(&c->alarms, PAPR_ALARM_CRIT_BATTERY);
            break;
        case PAPR_BATT_CRITICAL:
        case PAPR_BATT_CUTOFF:
            papr_alarms_set(&c->alarms, PAPR_ALARM_CRIT_BATTERY);
            break;
        default:
            papr_alarms_clear(&c->alarms, PAPR_ALARM_LOW_BATTERY);
            papr_alarms_clear(&c->alarms, PAPR_ALARM_CRIT_BATTERY);
            break;
    }

    /* Sensor health */
    if (!papr_sensors_all_valid(&c->sensors))
    {
        papr_alarms_set(&c->alarms, PAPR_ALARM_SENSOR_FAULT);
    }
    else
    {
        papr_alarms_clear(&c->alarms, PAPR_ALARM_SENSOR_FAULT);
    }

    /* Overtemp threshold: 70 °C on motor housing. */
    if (c->sensors.temperature_valid && c->sensors.temperature_c10 > 700)
    {
        papr_alarms_set(&c->alarms, PAPR_ALARM_OVERTEMP);
    }
    else
    {
        papr_alarms_clear(&c->alarms, PAPR_ALARM_OVERTEMP);
    }

    /* Motor fault: latched DIAG from the L6235 (overcurrent or thermal
     * shutdown), or commanded current > 50 % of IMAX with RPM near zero. */
    if (papr_blower_driver_fault(&c->blower) ||
        (papr_blower_duty_permille(&c->blower) > 500U &&
         papr_blower_rpm(&c->blower) < 500U))
    {
        papr_alarms_set(&c->alarms, PAPR_ALARM_MOTOR_FAULT);
    }

    papr_alarms_evaluate(&c->alarms,
                         c->sensors.flow_lpm,
                         papr_blower_duty_permille(&c->blower),
                         now_ms);
}

papr_status_t papr_controller_init(papr_controller_t *c)
{
    if (c == NULL) { return PAPR_ERR_PARAM; }

    papr_status_t s = papr_hal_init();
    if (s != PAPR_OK) { return s; }

    c->level            = PAPR_LEVEL_MED;
    c->runtime_seconds  = 0U;
    c->last_control_ms  = 0U;
    c->last_sensor_ms   = 0U;
    c->last_wdt_ms      = 0U;

    s = papr_blower_init(&c->blower);
    if (s != PAPR_OK) { return s; }
    s = papr_battery_init(&c->battery);
    if (s != PAPR_OK) { return s; }
    s = papr_sensors_init(&c->sensors);
    if (s != PAPR_OK) { return s; }
    papr_alarms_init(&c->alarms);

    /* BLE bring-up is best-effort; an absent module must not block the
     * supervisor from running locally. */
    (void)papr_ble_init(&c->ble);

    c->rc_power_on_pending    = false;
    c->rc_power_off_pending   = false;
    c->rc_level_pending       = false;
    c->rc_reset_fault_pending = false;
    c->rc_mute_pending_ms     = 0U;

    enter_state(c, PAPR_STATE_INIT);
    return PAPR_OK;
}

static void apply_remote_requests(papr_controller_t *c, uint32_t now)
{
    if (c->rc_mute_pending_ms != 0U)
    {
        papr_alarms_mute(&c->alarms, now, c->rc_mute_pending_ms);
        c->rc_mute_pending_ms = 0U;
    }

    if (c->rc_reset_fault_pending)
    {
        c->rc_reset_fault_pending = false;
        if (c->state == PAPR_STATE_FAULT)
        {
            c->alarms.active_mask  = 0U;
            c->alarms.latched_mask = 0U;
            enter_state(c, PAPR_STATE_INIT);
        }
    }

    if (c->rc_level_pending)
    {
        c->rc_level_pending = false;
        c->level            = c->rc_level_request;
        apply_level(c);
    }

    if (c->rc_power_on_pending)
    {
        c->rc_power_on_pending = false;
        if (c->state == PAPR_STATE_STANDBY)
        {
            apply_level(c);
            (void)papr_blower_start(&c->blower);
            enter_state(c, PAPR_STATE_RUNNING);
        }
    }

    if (c->rc_power_off_pending)
    {
        c->rc_power_off_pending = false;
        if (c->state == PAPR_STATE_RUNNING || c->state == PAPR_STATE_ALARM)
        {
            enter_state(c, PAPR_STATE_SHUTDOWN);
        }
    }
}

papr_status_t papr_controller_step(papr_controller_t *c)
{
    if (c == NULL) { return PAPR_ERR_PARAM; }

    uint32_t now = papr_hal_now_ms();

    if ((uint32_t)(now - c->last_wdt_ms) >= PAPR_WDT_KICK_MS)
    {
        papr_hal_wdt_kick();
        c->last_wdt_ms = now;
    }

    if ((uint32_t)(now - c->last_sensor_ms) >= PAPR_SENSOR_PERIOD_MS)
    {
        (void)papr_sensors_update(&c->sensors);
        (void)papr_battery_update(&c->battery);
        c->last_sensor_ms = now;
    }

    /* Service the wireless link first so any commands queued by the mobile
     * app are honoured in the same supervisor tick. */
    (void)papr_ble_poll(&c->ble, c);
    apply_remote_requests(c, now);

    switch (c->state)
    {
        case PAPR_STATE_INIT:
            enter_state(c, PAPR_STATE_SELF_TEST);
            break;

        case PAPR_STATE_SELF_TEST:
            if (run_self_test(c) == PAPR_OK)
            {
                enter_state(c, PAPR_STATE_STANDBY);
            }
            else if ((uint32_t)(now - c->state_entered_ms) > 3000U)
            {
                papr_alarms_set(&c->alarms, PAPR_ALARM_SENSOR_FAULT);
                enter_state(c, PAPR_STATE_FAULT);
            }
            break;

        case PAPR_STATE_STANDBY:
            (void)papr_blower_stop(&c->blower);
            if (papr_hal_button_power_pressed())
            {
                apply_level(c);
                (void)papr_blower_start(&c->blower);
                enter_state(c, PAPR_STATE_RUNNING);
            }
            break;

        case PAPR_STATE_RUNNING:
            if ((uint32_t)(now - c->last_control_ms) >= PAPR_CONTROL_PERIOD_MS)
            {
                uint32_t dt = (uint32_t)(now - c->last_control_ms);
                if (dt == 0U) { dt = PAPR_CONTROL_PERIOD_MS; }
                (void)papr_blower_update(&c->blower, c->sensors.flow_lpm, dt);
                c->last_control_ms = now;
            }
            evaluate_safety(c, now);
            if (papr_alarms_is_critical(&c->alarms))
            {
                enter_state(c, PAPR_STATE_ALARM);
            }
            if (papr_hal_button_level_pressed())
            {
                papr_controller_cycle_level(c);
            }
            if (papr_hal_button_power_pressed())
            {
                enter_state(c, PAPR_STATE_SHUTDOWN);
            }
            break;

        case PAPR_STATE_ALARM:
            /* Keep running so the user is not deprived of air, but the alarm
             * must remain audible. Only shutdown on cutoff or motor fault. */
            evaluate_safety(c, now);
            if ((c->alarms.active_mask & (uint32_t)PAPR_ALARM_MOTOR_FAULT) ||
                c->battery.level == PAPR_BATT_CUTOFF)
            {
                enter_state(c, PAPR_STATE_SHUTDOWN);
            }
            else if (!papr_alarms_is_critical(&c->alarms))
            {
                enter_state(c, PAPR_STATE_RUNNING);
            }
            else if ((uint32_t)(now - c->last_control_ms) >= PAPR_CONTROL_PERIOD_MS)
            {
                uint32_t dt = (uint32_t)(now - c->last_control_ms);
                if (dt == 0U) { dt = PAPR_CONTROL_PERIOD_MS; }
                (void)papr_blower_update(&c->blower, c->sensors.flow_lpm, dt);
                c->last_control_ms = now;
            }
            break;

        case PAPR_STATE_SHUTDOWN:
            (void)papr_blower_stop(&c->blower);
            enter_state(c, PAPR_STATE_STANDBY);
            break;

        case PAPR_STATE_FAULT:
        default:
            (void)papr_blower_stop(&c->blower);
            break;
    }

    papr_alarms_render(&c->alarms, now);
    return PAPR_OK;
}

void papr_controller_request_shutdown(papr_controller_t *c)
{
    if (c == NULL) { return; }
    enter_state(c, PAPR_STATE_SHUTDOWN);
}

void papr_controller_cycle_level(papr_controller_t *c)
{
    if (c == NULL) { return; }
    c->level = (papr_flow_level_t)(((unsigned)c->level + 1U) % (unsigned)PAPR_LEVEL_COUNT);
    apply_level(c);
}

void papr_controller_get_telemetry(const papr_controller_t *c,
                                   papr_telemetry_t *out)
{
    if (c == NULL || out == NULL) { return; }
    out->flow_lpm             = c->sensors.flow_lpm;
    out->pressure_pa          = c->sensors.pressure_pa;
    out->absolute_pressure_pa = c->sensors.absolute_pressure_pa;
    out->temperature_c10      = c->sensors.temperature_c10;
    out->battery_mv           = c->battery.voltage_mv;
    out->battery_ma           = c->battery.current_ma;
    out->battery_soc_percent  = c->battery.soc_percent;
    out->motor_rpm            = papr_blower_rpm(&c->blower);
    out->duty_permille        = papr_blower_duty_permille(&c->blower);
}

void papr_controller_snapshot(const papr_controller_t *c,
                              papr_telemetry_t *out_t,
                              uint8_t *out_state,
                              uint8_t *out_level,
                              uint32_t *out_alarms)
{
    if (c == NULL) { return; }
    if (out_t)      { papr_controller_get_telemetry(c, out_t); }
    if (out_state)  { *out_state  = (uint8_t)c->state; }
    if (out_level)  { *out_level  = (uint8_t)c->level; }
    if (out_alarms) { *out_alarms = c->alarms.active_mask; }
}

void papr_controller_remote_power(papr_controller_t *c, bool on)
{
    if (c == NULL) { return; }
    if (on) { c->rc_power_on_pending  = true; c->rc_power_off_pending = false; }
    else    { c->rc_power_off_pending = true; c->rc_power_on_pending  = false; }
}

void papr_controller_remote_set_level(papr_controller_t *c,
                                      papr_flow_level_t level)
{
    if (c == NULL || (unsigned)level >= (unsigned)PAPR_LEVEL_COUNT) { return; }
    c->rc_level_request = level;
    c->rc_level_pending = true;
}

void papr_controller_remote_mute(papr_controller_t *c, uint32_t duration_ms)
{
    if (c == NULL || duration_ms == 0U) { return; }
    c->rc_mute_pending_ms = duration_ms;
}

void papr_controller_remote_reset_fault(papr_controller_t *c)
{
    if (c == NULL) { return; }
    c->rc_reset_fault_pending = true;
}
