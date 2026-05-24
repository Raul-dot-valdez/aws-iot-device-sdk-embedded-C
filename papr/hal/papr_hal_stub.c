#include "papr_config.h"
#include "papr_gdy1124.h"
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

/* Per-slave register pointer. The GDY1124 transcript is "write register
 * address, then read N bytes" so we remember the last write to dispatch the
 * next read. Bench-validated BMP280-family calibration / raw values yield
 * ≈ 25 °C and ≈ 100 653 Pa, which keeps the host build's telemetry sane. */
static uint8_t s_gdy_reg_ptr;
static const uint8_t s_gdy_calib[PAPR_GDY1124_CALIB_LEN] = {
    /* T1=27504, T2=26435, T3=-1000 */
    0x70U, 0x6BU, 0x43U, 0x67U, 0x18U, 0xFCU,
    /* P1=36477, P2=-10685, P3=3024, P4=2855, P5=140, P6=-7,
     * P7=15500, P8=-14600, P9=6000 */
    0x7DU, 0x8EU, 0x43U, 0xD6U, 0xD0U, 0x0BU, 0x27U, 0x0BU, 0x8CU, 0x00U,
    0xF9U, 0xFFU, 0x8CU, 0x3CU, 0xF8U, 0xC6U, 0x70U, 0x17U
};
static const uint8_t s_gdy_raw[6] = {
    /* adc_P = 415148  →  0x6585C  packed in [19..0] of three bytes      */
    0x65U, 0x85U, 0xC0U,
    /* adc_T = 519888  →  0x7EE34                                        */
    0x7EU, 0xE3U, 0x40U
};

papr_status_t papr_hal_i2c_write(uint8_t addr7, const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0U) { return PAPR_ERR_PARAM; }

    if (addr7 == PAPR_SDP810_I2C_ADDR || addr7 == 0x00U)
    {
        /* SDP810 commands. */
        if (len >= 2U && data[0] == 0x36U && data[1] == 0x1EU)
        {
            s_sdp_started  = true;
            s_sdp_raw_temp = (int16_t)(25 * 200);
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

    if (addr7 == PAPR_GDY1124_I2C_ADDR)
    {
        /* First byte is always the register pointer; subsequent bytes are
         * register writes which we silently accept. */
        s_gdy_reg_ptr = data[0];
        return PAPR_OK;
    }

    return PAPR_ERR_HW;
}

papr_status_t papr_hal_i2c_read(uint8_t addr7, uint8_t *data, size_t len)
{
    if (data == NULL) { return PAPR_ERR_PARAM; }

    if (addr7 == PAPR_SDP810_I2C_ADDR)
    {
        if (!s_sdp_started || len < 9U) { return PAPR_ERR_HW; }
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

    if (addr7 == PAPR_GDY1124_I2C_ADDR)
    {
        if (s_gdy_reg_ptr == PAPR_GDY1124_REG_CHIP_ID && len >= 1U)
        {
            data[0] = PAPR_GDY1124_CHIP_ID;
            if (len > 1U) { memset(data + 1, 0, len - 1U); }
            return PAPR_OK;
        }
        if (s_gdy_reg_ptr == PAPR_GDY1124_REG_CALIB_BASE &&
            len >= PAPR_GDY1124_CALIB_LEN)
        {
            memcpy(data, s_gdy_calib, PAPR_GDY1124_CALIB_LEN);
            if (len > PAPR_GDY1124_CALIB_LEN)
            {
                memset(data + PAPR_GDY1124_CALIB_LEN, 0,
                       len - PAPR_GDY1124_CALIB_LEN);
            }
            return PAPR_OK;
        }
        if (s_gdy_reg_ptr == PAPR_GDY1124_REG_PRESS_MSB && len >= 6U)
        {
            memcpy(data, s_gdy_raw, 6U);
            if (len > 6U) { memset(data + 6, 0, len - 6U); }
            return PAPR_OK;
        }
        memset(data, 0, len);
        return PAPR_OK;
    }

    return PAPR_ERR_HW;
}

/* ---- UART / BLE control stubs --------------------------------------------
 * The host build does not actually attach a BLE module; UART writes are
 * dropped and the receive buffer is always empty. This keeps the firmware
 * exercising the BLE poll path without simulating phone-side traffic. */

papr_status_t papr_hal_uart_write(const uint8_t *data, size_t len)
{
    (void)data;
    (void)len;
    return PAPR_OK;
}

bool papr_hal_uart_read_byte(uint8_t *out)
{
    (void)out;
    return false;
}

papr_status_t papr_hal_ble_set_reset(bool asserted)
{
    (void)asserted;
    return PAPR_OK;
}

bool papr_hal_ble_host_wake(void) { return false; }

/* ---- OTA flash stub ------------------------------------------------------
 * A RAM buffer stands in for the inactive flash slot so the OTA receive /
 * verify path runs end-to-end on the host. commit() records the metadata and
 * reboot() is a no-op (the host process keeps running). */

static uint8_t  s_ota_slot[PAPR_OTA_SLOT_SIZE];
static uint32_t s_ota_committed_size;
static uint32_t s_ota_committed_crc;
static bool     s_ota_pending;

uint32_t papr_hal_ota_slot_size(void)
{
    return (uint32_t)sizeof(s_ota_slot);
}

papr_status_t papr_hal_ota_erase(void)
{
    memset(s_ota_slot, 0xFF, sizeof(s_ota_slot));
    return PAPR_OK;
}

papr_status_t papr_hal_ota_write(uint32_t offset, const uint8_t *data, uint32_t len)
{
    if (data == NULL) { return PAPR_ERR_PARAM; }
    if ((uint64_t)offset + len > sizeof(s_ota_slot)) { return PAPR_ERR_PARAM; }
    memcpy(&s_ota_slot[offset], data, len);
    return PAPR_OK;
}

papr_status_t papr_hal_ota_read(uint32_t offset, uint8_t *data, uint32_t len)
{
    if (data == NULL) { return PAPR_ERR_PARAM; }
    if ((uint64_t)offset + len > sizeof(s_ota_slot)) { return PAPR_ERR_PARAM; }
    memcpy(data, &s_ota_slot[offset], len);
    return PAPR_OK;
}

papr_status_t papr_hal_ota_commit(uint32_t size, uint32_t crc32)
{
    s_ota_committed_size = size;
    s_ota_committed_crc  = crc32;
    s_ota_pending        = true;
    return PAPR_OK;
}

void papr_hal_ota_reboot(void)
{
    /* On real hardware this is NVIC_SystemReset(); the host keeps running so
     * tests can inspect the committed metadata. */
    (void)s_ota_committed_size;
    (void)s_ota_committed_crc;
    (void)s_ota_pending;
}

/* ---- Production test / provisioning stub ---------------------------------
 * The host never enters factory mode (would block the simulation in the
 * command loop). The provisioning page is RAM-backed; unique_id is a fixed
 * pattern so a host build still produces a deterministic fallback serial. */

static uint8_t s_prov_page[64];

bool papr_hal_factory_requested(void)
{
    return false;
}

void papr_hal_unique_id(uint8_t out[12])
{
    if (out == NULL) { return; }
    for (uint8_t i = 0U; i < 12U; ++i) { out[i] = (uint8_t)(0xA0U + i); }
}

papr_status_t papr_hal_prov_read(uint8_t *data, uint32_t len)
{
    if (data == NULL || len > sizeof(s_prov_page)) { return PAPR_ERR_PARAM; }
    memcpy(data, s_prov_page, len);
    return PAPR_OK;
}

papr_status_t papr_hal_prov_write(const uint8_t *data, uint32_t len)
{
    if (data == NULL || len > sizeof(s_prov_page)) { return PAPR_ERR_PARAM; }
    memcpy(s_prov_page, data, len);
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

papr_status_t papr_hal_keypad_drive_row(uint8_t row, bool active)
{
    (void)row;
    (void)active;
    return PAPR_OK;
}

bool papr_hal_keypad_read_col(uint8_t col)
{
    (void)col;
    return false;
}

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
