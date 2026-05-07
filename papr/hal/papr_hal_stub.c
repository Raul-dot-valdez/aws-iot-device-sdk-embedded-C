#include "papr_config.h"
#include "papr_hal.h"
#include "papr_sdp810.h"

#include <string.h>

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

/* ---- I²C transport stub --------------------------------------------------
 * For host builds we simulate a Sensirion SDP810 attached at 0x25. The
 * "device" returns a synthetic differential pressure proportional to the
 * blower demand, with proper Sensirion CRC-8 framing so the portable driver
 * accepts the result. Any other slave address is rejected with PAPR_ERR_HW.
 */

static bool     s_sdp_started;
static int16_t  s_sdp_raw_dp;
static int16_t  s_sdp_raw_temp;

papr_status_t papr_hal_i2c_write(uint8_t addr7, const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0U) { return PAPR_ERR_PARAM; }
    if (addr7 != PAPR_SDP810_I2C_ADDR && addr7 != 0x00U) { return PAPR_ERR_HW; }

    /* Treat any 2-byte 0x36 0x1E as "start continuous DP w/ averaging" and
     * 0x3F 0xF9 as "stop". Update the simulated raw values from the latest
     * blower demand so the rest of the firmware sees consistent telemetry. */
    if (len >= 2U && data[0] == 0x36U && data[1] == 0x1EU)
    {
        s_sdp_started  = true;
        s_sdp_raw_temp = (int16_t)(25 * 200); /* 25.00 °C */
    }
    else if (len >= 2U && data[0] == 0x3FU && data[1] == 0xF9U)
    {
        s_sdp_started = false;
    }
    int32_t pa = (int32_t)(((uint32_t)s_vref_code * 200U) /
                           PAPR_L6235_VREF_DAC_MAX);
    s_sdp_raw_dp = (int16_t)(pa * (int32_t)PAPR_SDP810_SCALE);
    return PAPR_OK;
}

papr_status_t papr_hal_i2c_read(uint8_t addr7, uint8_t *data, size_t len)
{
    if (data == NULL) { return PAPR_ERR_PARAM; }
    if (addr7 != PAPR_SDP810_I2C_ADDR) { return PAPR_ERR_HW; }
    if (!s_sdp_started || len < 9U)    { return PAPR_ERR_HW; }

    uint8_t frame[9];
    frame[0] = (uint8_t)((uint16_t)s_sdp_raw_dp >> 8);
    frame[1] = (uint8_t)((uint16_t)s_sdp_raw_dp & 0xFFU);
    frame[2] = papr_sdp810_crc8(&frame[0], 2U);
    frame[3] = (uint8_t)((uint16_t)s_sdp_raw_temp >> 8);
    frame[4] = (uint8_t)((uint16_t)s_sdp_raw_temp & 0xFFU);
    frame[5] = papr_sdp810_crc8(&frame[3], 2U);
    frame[6] = (uint8_t)((uint16_t)PAPR_SDP810_SCALE >> 8);
    frame[7] = (uint8_t)((uint16_t)PAPR_SDP810_SCALE & 0xFFU);
    frame[8] = papr_sdp810_crc8(&frame[6], 2U);

    memcpy(data, frame, 9U);
    if (len > 9U) { memset(data + 9U, 0, len - 9U); }
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
