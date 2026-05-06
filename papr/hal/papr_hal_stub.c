#include "papr_config.h"
#include "papr_hal.h"

/* Reference HAL stub. Replace each routine with vendor SDK calls (STM32 HAL,
 * NXP MCUXpresso, Zephyr, ESP-IDF ...) on the target platform. The stub keeps
 * the system compilable for host-side validation and unit tests.
 *
 * Mapping for an L6235 reference design on, e.g., STM32F4:
 *   VREF  ← DAC1_OUT1 (or TIM PWM through an RC filter)
 *   EN    ← GPIO output, push-pull
 *   FWD   ← GPIO output, push-pull
 *   BRAKE ← GPIO output, push-pull, default HIGH (brake engaged at boot)
 *   DIAG  ← GPIO input, internal pull-up, EXTI on falling edge
 *   TACHO ← TIM input capture, channel in PWM-input mode
 */

static uint16_t s_vref_code;
static uint16_t s_simulated_rpm;
static bool     s_enabled;
static bool     s_forward;
static bool     s_brake;
static bool     s_diag_active;
static uint32_t s_now_ms;

papr_status_t papr_hal_init(void)
{
    s_vref_code     = 0U;
    s_simulated_rpm = 0U;
    s_enabled       = false;
    s_forward       = true;
    s_brake         = true;
    s_diag_active   = false;
    s_now_ms        = 0U;
    return PAPR_OK;
}

uint32_t papr_hal_now_ms(void)
{
    return ++s_now_ms;
}

papr_status_t papr_hal_l6235_set_vref(uint16_t code)
{
    s_vref_code = (code > PAPR_L6235_VREF_DAC_MAX) ? PAPR_L6235_VREF_DAC_MAX : code;
    /* Crude motor model: RPM tracks current setpoint when enabled. */
    if (s_enabled && !s_brake)
    {
        s_simulated_rpm = (uint16_t)(((uint32_t)s_vref_code * 32000U)
                                     / PAPR_L6235_VREF_DAC_MAX);
    }
    else
    {
        s_simulated_rpm = 0U;
    }
    return PAPR_OK;
}

papr_status_t papr_hal_l6235_set_enable(bool enable)
{
    s_enabled = enable;
    if (!enable) { s_simulated_rpm = 0U; }
    return PAPR_OK;
}

papr_status_t papr_hal_l6235_set_forward(bool forward)
{
    s_forward = forward;
    return PAPR_OK;
}

papr_status_t papr_hal_l6235_set_brake(bool brake_engaged)
{
    s_brake = brake_engaged;
    if (brake_engaged) { s_simulated_rpm = 0U; }
    return PAPR_OK;
}

bool papr_hal_l6235_diag_active(void)
{
    return s_diag_active;
}

papr_status_t papr_hal_l6235_read_tacho_rpm(uint16_t *out)
{
    if (out == NULL) { return PAPR_ERR_PARAM; }
    *out = s_simulated_rpm;
    return PAPR_OK;
}

papr_status_t papr_hal_read_flow_lpm(uint16_t *out)
{
    if (out == NULL) { return PAPR_ERR_PARAM; }
    /* Linear current-to-flow approximation for simulation. */
    uint32_t lpm = ((uint32_t)s_vref_code * 250U) / PAPR_L6235_VREF_DAC_MAX;
    *out = (s_enabled && !s_brake) ? (uint16_t)lpm : 0U;
    return PAPR_OK;
}

papr_status_t papr_hal_read_pressure_pa(uint16_t *out)
{
    if (out == NULL) { return PAPR_ERR_PARAM; }
    *out = (uint16_t)(((uint32_t)s_vref_code * 800U) / PAPR_L6235_VREF_DAC_MAX);
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
    *out = (uint16_t)(((uint32_t)s_vref_code * 4000U) / PAPR_L6235_VREF_DAC_MAX);
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
