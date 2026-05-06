#include "papr_config.h"
#include "papr_hal.h"

/* Reference HAL stub. Replace each routine with vendor SDK calls (STM32 HAL,
 * NXP MCUXpresso, Zephyr, ESP-IDF ...) on the target platform. The stub keeps
 * the system compilable for host-side validation and unit tests. */

static uint16_t s_duty;
static bool     s_blower_on;
static uint32_t s_now_ms;

papr_status_t papr_hal_init(void)
{
    s_duty      = 0U;
    s_blower_on = false;
    s_now_ms    = 0U;
    return PAPR_OK;
}

uint32_t papr_hal_now_ms(void)
{
    return ++s_now_ms;
}

papr_status_t papr_hal_blower_set_duty(uint16_t duty)
{
    s_duty = (duty > PAPR_PWM_MAX) ? PAPR_PWM_MAX : duty;
    return PAPR_OK;
}

papr_status_t papr_hal_blower_enable(bool enable)
{
    s_blower_on = enable;
    return PAPR_OK;
}

papr_status_t papr_hal_read_flow_lpm(uint16_t *out)
{
    if (out == NULL) { return PAPR_ERR_PARAM; }
    /* Linear duty-to-flow approximation for simulation. */
    uint32_t lpm = ((uint32_t)s_duty * 250U) / PAPR_PWM_MAX;
    *out = s_blower_on ? (uint16_t)lpm : 0U;
    return PAPR_OK;
}

papr_status_t papr_hal_read_pressure_pa(uint16_t *out)
{
    if (out == NULL) { return PAPR_ERR_PARAM; }
    *out = (uint16_t)(((uint32_t)s_duty * 800U) / PAPR_PWM_MAX);
    return PAPR_OK;
}

papr_status_t papr_hal_read_battery_mv(uint16_t *out)
{
    if (out == NULL) { return PAPR_ERR_PARAM; }
    *out = 15800U;
    return PAPR_OK;
}

papr_status_t papr_hal_read_battery_ma(uint16_t *out)
{
    if (out == NULL) { return PAPR_ERR_PARAM; }
    *out = (uint16_t)(((uint32_t)s_duty * 4000U) / PAPR_PWM_MAX);
    return PAPR_OK;
}

papr_status_t papr_hal_read_motor_rpm(uint16_t *out)
{
    if (out == NULL) { return PAPR_ERR_PARAM; }
    *out = (uint16_t)(((uint32_t)s_duty * 32000U) / PAPR_PWM_MAX);
    return PAPR_OK;
}

papr_status_t papr_hal_read_temperature_c10(int16_t *out)
{
    if (out == NULL) { return PAPR_ERR_PARAM; }
    *out = 350; /* 35.0 °C */
    return PAPR_OK;
}

bool papr_hal_button_power_pressed(void) { return false; }
bool papr_hal_button_level_pressed(void) { return false; }

papr_status_t papr_hal_led_set(uint8_t led_id, bool on)
{
    (void)led_id;
    (void)on;
    return PAPR_OK;
}

papr_status_t papr_hal_buzzer_set(bool on, uint16_t freq_hz)
{
    (void)on;
    (void)freq_hz;
    return PAPR_OK;
}

void papr_hal_wdt_kick(void) { /* no-op on host */ }
