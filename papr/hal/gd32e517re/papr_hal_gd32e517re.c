/* HAL implementation for the GigaDevice GD32E517RE.
 *
 *   Core      : Cortex-M33 with FPU, 180 MHz from a 25 MHz HXTAL via PLL
 *   Package   : LQFP64 (51 GPIOs)
 *   Flash/RAM : 512 KB / 128 KB
 *
 * The file targets the official GD32E51x firmware library (`gd32e51x.h` and
 * the standard peripheral drivers). It is *not* part of the host build —
 * CMake includes it only when PAPR_TARGET=GD32E517RE, which also pulls in
 * the GigaDevice startup file and linker script supplied via GD32E51X_SDK_DIR. */

#ifdef GD32E51X

#include "gd32e51x.h"
#include "papr_config.h"
#include "papr_hal.h"
#include "papr_pinmap.h"

/* ------------------------------------------------------------------------- */
/* Globals                                                                   */
/* ------------------------------------------------------------------------- */

static volatile uint32_t s_tick_ms;
static volatile bool     s_diag_pending;
static volatile uint32_t s_diag_edge_ms;
static volatile uint16_t s_tacho_rpm;

/* Tacho counter sampling window. */
#define TACHO_SAMPLE_PERIOD_MS  100U

/* ADC reference (VDDA, mV) and resolution. */
#define ADC_VREF_MV             3300U
#define ADC_FULL_SCALE          4095U

/* Battery divider: pack 16.8 V → ADC ≤ 3.3 V → ratio = (R1+R2)/R2 = 220/43.   */
#define VBAT_DIV_NUM            220U
#define VBAT_DIV_DEN            43U

/* Shunt amp: 50 mΩ shunt, ×50 amplifier → 2.5 V/A → 1 mA = 2.5 ADC counts.  */
#define IBAT_MA_PER_COUNT_NUM   1U
#define IBAT_MA_PER_COUNT_DEN   3U      /* ≈ ADC_VREF / (full_scale * 2.5)  */

/* Flow sensor: 0–5 V mapped through divider to 0–3.0 V on ADC, full-scale  */
/* corresponds to 250 LPM.                                                  */
#define FLOW_FULL_SCALE_LPM     250U

/* ------------------------------------------------------------------------- */
/* Clock and SysTick                                                         */
/* ------------------------------------------------------------------------- */

static void clock_init(void)
{
    /* SystemInit() (vendor) brings the PLL up to 180 MHz from HXTAL.       */
    SysTick_Config(SystemCoreClock / 1000U);
    NVIC_SetPriority(SysTick_IRQn, 0U);
}

void SysTick_Handler(void)
{
    ++s_tick_ms;

    /* Periodically latch the TACHO accumulator into an RPM estimate.       */
    static uint32_t s_last_sample_ms;
    static uint32_t s_last_count;
    if ((s_tick_ms - s_last_sample_ms) >= TACHO_SAMPLE_PERIOD_MS)
    {
        uint32_t count  = (uint32_t)timer_counter_read(PAPR_TACHO_TIMER);
        uint32_t delta  = (uint32_t)(count - s_last_count) & 0xFFFFU;
        s_last_count    = count;
        s_last_sample_ms = s_tick_ms;

        /* RPM = pulses_per_sec * 60 / PPR
         *     = (delta / 0.1 s) * 60 / PPR
         *     = delta * 600 / PPR                                         */
        uint32_t rpm = (delta * 600U) / PAPR_L6235_TACHO_PPR;
        if (rpm > 0xFFFFU) { rpm = 0xFFFFU; }
        s_tacho_rpm = (uint16_t)rpm;
    }
}

uint32_t papr_hal_now_ms(void)
{
    return s_tick_ms;
}

/* ------------------------------------------------------------------------- */
/* GPIO bring-up                                                             */
/* ------------------------------------------------------------------------- */

static void gpio_clocks_enable(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_AF);
}

static void l6235_pins_init(void)
{
    gpio_init(PAPR_PIN_L6235_EN_PORT,    GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ,
              PAPR_PIN_L6235_EN_PIN);
    gpio_init(PAPR_PIN_L6235_FWD_PORT,   GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ,
              PAPR_PIN_L6235_FWD_PIN);
    gpio_init(PAPR_PIN_L6235_BRAKE_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ,
              PAPR_PIN_L6235_BRAKE_PIN);

    /* Defaults: bridge disabled, brake engaged, direction = forward.       */
    gpio_bit_reset(PAPR_PIN_L6235_EN_PORT,    PAPR_PIN_L6235_EN_PIN);
    gpio_bit_reset(PAPR_PIN_L6235_BRAKE_PORT, PAPR_PIN_L6235_BRAKE_PIN);
    gpio_bit_set(  PAPR_PIN_L6235_FWD_PORT,   PAPR_PIN_L6235_FWD_PIN);

    /* DIAG input with internal pull-up. */
    gpio_init(PAPR_PIN_L6235_DIAG_PORT, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ,
              PAPR_PIN_L6235_DIAG_PIN);
    gpio_exti_source_select(PAPR_PIN_L6235_DIAG_EXTI_PORT_SRC,
                            PAPR_PIN_L6235_DIAG_EXTI_SRC);
    exti_init(PAPR_PIN_L6235_DIAG_EXTI, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    exti_interrupt_flag_clear(PAPR_PIN_L6235_DIAG_EXTI);
    nvic_irq_enable(EXTI10_15_IRQn, 1U, 0U);

    /* TACHO input as TIMER1_CH0 alternate function. */
    gpio_init(PAPR_PIN_L6235_TACHO_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ,
              PAPR_PIN_L6235_TACHO_PIN);
}

static void ui_pins_init(void)
{
    gpio_init(PAPR_PIN_BTN_POWER_PORT, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ,
              PAPR_PIN_BTN_POWER_PIN);
    gpio_init(PAPR_PIN_BTN_LEVEL_PORT, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ,
              PAPR_PIN_BTN_LEVEL_PIN);

    gpio_init(PAPR_PIN_LED_OK_PORT,    GPIO_MODE_OUT_PP, GPIO_OSPEED_2MHZ,
              PAPR_PIN_LED_OK_PIN);
    gpio_init(PAPR_PIN_LED_WARN_PORT,  GPIO_MODE_OUT_PP, GPIO_OSPEED_2MHZ,
              PAPR_PIN_LED_WARN_PIN);
    gpio_init(PAPR_PIN_LED_FAULT_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_2MHZ,
              PAPR_PIN_LED_FAULT_PIN);
}

/* ------------------------------------------------------------------------- */
/* DAC for L6235 VREF                                                        */
/* ------------------------------------------------------------------------- */

static void dac_init_vref(void)
{
    rcu_periph_clock_enable(RCU_DAC);
    gpio_init(PAPR_PIN_L6235_VREF_PORT, GPIO_MODE_AIN, GPIO_OSPEED_50MHZ,
              PAPR_PIN_L6235_VREF_PIN);

    dac_deinit();
    dac_trigger_disable(PAPR_DAC_CHANNEL);
    dac_wave_mode_config(PAPR_DAC_CHANNEL, DAC_WAVE_DISABLE);
    dac_output_buffer_enable(PAPR_DAC_CHANNEL);
    dac_data_set(PAPR_DAC_CHANNEL, DAC_ALIGN_12B_R, 0U);
    dac_enable(PAPR_DAC_CHANNEL);
}

papr_status_t papr_hal_l6235_set_vref(uint16_t code)
{
    if (code > PAPR_L6235_VREF_DAC_MAX) { code = PAPR_L6235_VREF_DAC_MAX; }
    /* Scale the controller's 0..PAPR_L6235_VREF_DAC_MAX range into the
     * 12-bit DAC register. They match (4095) by default but the stage stays
     * here in case the controller is rebuilt with a coarser resolution. */
    uint32_t dac_code = ((uint32_t)code * 4095U) / PAPR_L6235_VREF_DAC_MAX;
    dac_data_set(PAPR_DAC_CHANNEL, DAC_ALIGN_12B_R, (uint16_t)dac_code);
    return PAPR_OK;
}

papr_status_t papr_hal_l6235_set_enable(bool enable)
{
    if (enable) { gpio_bit_set(PAPR_PIN_L6235_EN_PORT, PAPR_PIN_L6235_EN_PIN); }
    else        { gpio_bit_reset(PAPR_PIN_L6235_EN_PORT, PAPR_PIN_L6235_EN_PIN); }
    return PAPR_OK;
}

papr_status_t papr_hal_l6235_set_forward(bool forward)
{
    if (forward) { gpio_bit_set(PAPR_PIN_L6235_FWD_PORT, PAPR_PIN_L6235_FWD_PIN); }
    else         { gpio_bit_reset(PAPR_PIN_L6235_FWD_PORT, PAPR_PIN_L6235_FWD_PIN); }
    return PAPR_OK;
}

papr_status_t papr_hal_l6235_set_brake(bool brake_engaged)
{
    /* BRAKE on the L6235 is active LOW. */
    if (brake_engaged) { gpio_bit_reset(PAPR_PIN_L6235_BRAKE_PORT, PAPR_PIN_L6235_BRAKE_PIN); }
    else               { gpio_bit_set(PAPR_PIN_L6235_BRAKE_PORT,   PAPR_PIN_L6235_BRAKE_PIN); }
    return PAPR_OK;
}

bool papr_hal_l6235_diag_active(void)
{
    if (!s_diag_pending) { return false; }
    /* Debounce: ignore edges shorter than DIAG_DEBOUNCE_MS. */
    if ((s_tick_ms - s_diag_edge_ms) < PAPR_L6235_DIAG_DEBOUNCE_MS)
    {
        return false;
    }
    bool low = (gpio_input_bit_get(PAPR_PIN_L6235_DIAG_PORT,
                                   PAPR_PIN_L6235_DIAG_PIN) == RESET);
    if (!low) { s_diag_pending = false; }
    return low;
}

void EXTI10_15_IRQHandler(void)
{
    if (exti_interrupt_flag_get(PAPR_PIN_L6235_DIAG_EXTI) != RESET)
    {
        exti_interrupt_flag_clear(PAPR_PIN_L6235_DIAG_EXTI);
        s_diag_pending = true;
        s_diag_edge_ms = s_tick_ms;
    }
}

/* ------------------------------------------------------------------------- */
/* TACHO counter                                                             */
/* ------------------------------------------------------------------------- */

static void tacho_timer_init(void)
{
    rcu_periph_clock_enable(RCU_TIMER1);
    timer_deinit(PAPR_TACHO_TIMER);

    timer_parameter_struct cfg;
    timer_struct_para_init(&cfg);
    cfg.prescaler         = 0U;
    cfg.alignedmode       = TIMER_COUNTER_EDGE;
    cfg.counterdirection  = TIMER_COUNTER_UP;
    cfg.period            = 0xFFFFU;
    cfg.clockdivision     = TIMER_CKDIV_DIV1;
    timer_init(PAPR_TACHO_TIMER, &cfg);

    timer_slave_mode_select(PAPR_TACHO_TIMER, TIMER_SLAVE_MODE_EXTERNAL0);
    timer_input_trigger_source_select(PAPR_TACHO_TIMER, TIMER_SMCFG_TRGSEL_CI0FE0);
    timer_input_capture_struct ic;
    ic.icpolarity   = TIMER_IC_POLARITY_RISING;
    ic.icselection  = TIMER_IC_SELECTION_DIRECTTI;
    ic.icprescaler  = TIMER_IC_PSC_DIV1;
    ic.icfilter     = 0x4U;
    timer_input_capture_config(PAPR_TACHO_TIMER, TIMER_CH_0, &ic);
    timer_enable(PAPR_TACHO_TIMER);
}

papr_status_t papr_hal_l6235_read_tacho_rpm(uint16_t *out)
{
    if (out == NULL) { return PAPR_ERR_PARAM; }
    *out = s_tacho_rpm;
    return PAPR_OK;
}

/* ------------------------------------------------------------------------- */
/* ADC for battery, flow, temperature                                        */
/* ------------------------------------------------------------------------- */

static void adc_init_papr(void)
{
    rcu_periph_clock_enable(RCU_ADC0);
    rcu_adc_clock_config(RCU_CKADC_CKAPB2_DIV6); /* 30 MHz max ADC clock   */

    gpio_init(PAPR_PIN_VBAT_PORT, GPIO_MODE_AIN, GPIO_OSPEED_50MHZ, PAPR_PIN_VBAT_PIN);
    gpio_init(PAPR_PIN_IBAT_PORT, GPIO_MODE_AIN, GPIO_OSPEED_50MHZ, PAPR_PIN_IBAT_PIN);
    gpio_init(PAPR_PIN_FLOW_PORT, GPIO_MODE_AIN, GPIO_OSPEED_50MHZ, PAPR_PIN_FLOW_PIN);
    gpio_init(PAPR_PIN_TEMP_PORT, GPIO_MODE_AIN, GPIO_OSPEED_50MHZ, PAPR_PIN_TEMP_PIN);

    adc_deinit(ADC0);
    adc_mode_config(ADC_MODE_FREE);
    adc_data_alignment_config(ADC0, ADC_DATAALIGN_RIGHT);
    adc_resolution_config(ADC0, ADC_RESOLUTION_12B);
    adc_external_trigger_source_config(ADC0, ADC_REGULAR_CHANNEL,
                                       ADC0_1_EXTTRIG_REGULAR_NONE);
    adc_external_trigger_config(ADC0, ADC_REGULAR_CHANNEL, ENABLE);
    adc_special_function_config(ADC0, ADC_CONTINUOUS_MODE, DISABLE);
    adc_special_function_config(ADC0, ADC_SCAN_MODE, DISABLE);
    adc_enable(ADC0);
    delay_1ms(1U);
    adc_calibration_enable(ADC0);
}

static uint16_t adc_sample(uint8_t channel)
{
    adc_regular_channel_config(ADC0, 0U, channel, ADC_SAMPLETIME_239POINT5);
    adc_software_trigger_enable(ADC0, ADC_REGULAR_CHANNEL);
    while (adc_flag_get(ADC0, ADC_FLAG_EOC) == RESET) { /* spin */ }
    adc_flag_clear(ADC0, ADC_FLAG_EOC);
    return (uint16_t)adc_regular_data_read(ADC0);
}

papr_status_t papr_hal_read_battery_mv(uint16_t *out)
{
    if (out == NULL) { return PAPR_ERR_PARAM; }
    uint32_t code = adc_sample((uint8_t)PAPR_ADC_VBAT_CH);
    uint32_t mv   = (code * ADC_VREF_MV) / ADC_FULL_SCALE;
    mv            = (mv * VBAT_DIV_NUM) / VBAT_DIV_DEN;
    if (mv > 0xFFFFU) { mv = 0xFFFFU; }
    *out = (uint16_t)mv;
    return PAPR_OK;
}

papr_status_t papr_hal_read_battery_ma(uint16_t *out)
{
    if (out == NULL) { return PAPR_ERR_PARAM; }
    uint32_t code = adc_sample((uint8_t)PAPR_ADC_IBAT_CH);
    uint32_t ma   = (code * IBAT_MA_PER_COUNT_NUM) / IBAT_MA_PER_COUNT_DEN;
    if (ma > 0xFFFFU) { ma = 0xFFFFU; }
    *out = (uint16_t)ma;
    return PAPR_OK;
}

papr_status_t papr_hal_read_flow_lpm(uint16_t *out)
{
    if (out == NULL) { return PAPR_ERR_PARAM; }
    uint32_t code = adc_sample((uint8_t)PAPR_ADC_FLOW_CH);
    uint32_t lpm  = (code * FLOW_FULL_SCALE_LPM) / ADC_FULL_SCALE;
    *out = (uint16_t)lpm;
    return PAPR_OK;
}

papr_status_t papr_hal_read_temperature_c10(int16_t *out)
{
    if (out == NULL) { return PAPR_ERR_PARAM; }
    /* Beta-equation conversion for a 10 kΩ NTC, β = 3950, in series with a
     * 10 kΩ pull-up to VDDA. Approximate around 25 °C using a precomputed
     * lookup; replace with a full table for accuracy below 10 °C / above
     * 60 °C. */
    uint32_t code = adc_sample((uint8_t)PAPR_ADC_TEMP_CH);
    int32_t mv    = (int32_t)((code * ADC_VREF_MV) / ADC_FULL_SCALE);
    int32_t c10   = 250 + ((1500 - mv) / 18); /* ≈ 5.6 mV/°C around 25 °C */
    if (c10 < -400) { c10 = -400; }
    if (c10 >  900) { c10 =  900; }
    *out = (int16_t)c10;
    return PAPR_OK;
}

/* ------------------------------------------------------------------------- */
/* I²C transport (used by the portable Sensirion SDP810 driver)              */
/* ------------------------------------------------------------------------- */

#define I2C_TIMEOUT_LOOPS  200000U

static void i2c_bus_init(void)
{
    rcu_periph_clock_enable(RCU_I2C0);
    gpio_init(PAPR_PIN_I2C_SCL_PORT, GPIO_MODE_AF_OD, GPIO_OSPEED_50MHZ,
              PAPR_PIN_I2C_SCL_PIN);
    gpio_init(PAPR_PIN_I2C_SDA_PORT, GPIO_MODE_AF_OD, GPIO_OSPEED_50MHZ,
              PAPR_PIN_I2C_SDA_PIN);

    i2c_deinit(PAPR_I2C_PERIPH);
    i2c_clock_config(PAPR_I2C_PERIPH, 400000U, I2C_DTCY_2);
    i2c_mode_addr_config(PAPR_I2C_PERIPH, I2C_I2CMODE_ENABLE,
                         I2C_ADDFORMAT_7BITS, 0U);
    i2c_enable(PAPR_I2C_PERIPH);
    i2c_ack_config(PAPR_I2C_PERIPH, I2C_ACK_ENABLE);
}

static papr_status_t wait_flag(uint32_t flag)
{
    uint32_t t = I2C_TIMEOUT_LOOPS;
    while (!i2c_flag_get(PAPR_I2C_PERIPH, flag) && --t) { }
    return (t == 0U) ? PAPR_ERR_TIMEOUT : PAPR_OK;
}

static papr_status_t wait_busy_clear(void)
{
    uint32_t t = I2C_TIMEOUT_LOOPS;
    while (i2c_flag_get(PAPR_I2C_PERIPH, I2C_FLAG_I2CBSY) && --t) { }
    return (t == 0U) ? PAPR_ERR_TIMEOUT : PAPR_OK;
}

papr_status_t papr_hal_i2c_write(uint8_t addr7, const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0U) { return PAPR_ERR_PARAM; }
    papr_status_t s = wait_busy_clear();
    if (s != PAPR_OK) { return s; }

    i2c_start_on_bus(PAPR_I2C_PERIPH);
    s = wait_flag(I2C_FLAG_SBSEND);
    if (s != PAPR_OK) { return s; }

    i2c_master_addressing(PAPR_I2C_PERIPH, (uint32_t)(addr7 << 1),
                          I2C_TRANSMITTER);
    s = wait_flag(I2C_FLAG_ADDSEND);
    if (s != PAPR_OK) { return s; }
    i2c_flag_clear(PAPR_I2C_PERIPH, I2C_FLAG_ADDSEND);

    for (size_t i = 0U; i < len; ++i)
    {
        s = wait_flag(I2C_FLAG_TBE);
        if (s != PAPR_OK) { i2c_stop_on_bus(PAPR_I2C_PERIPH); return s; }
        i2c_data_transmit(PAPR_I2C_PERIPH, data[i]);
    }
    s = wait_flag(I2C_FLAG_BTC);
    if (s != PAPR_OK) { i2c_stop_on_bus(PAPR_I2C_PERIPH); return s; }

    i2c_stop_on_bus(PAPR_I2C_PERIPH);
    return PAPR_OK;
}

papr_status_t papr_hal_i2c_read(uint8_t addr7, uint8_t *data, size_t len)
{
    if (data == NULL || len == 0U) { return PAPR_ERR_PARAM; }
    papr_status_t s = wait_busy_clear();
    if (s != PAPR_OK) { return s; }

    i2c_ack_config(PAPR_I2C_PERIPH, I2C_ACK_ENABLE);
    i2c_ackpos_config(PAPR_I2C_PERIPH, I2C_ACKPOS_CURRENT);
    i2c_start_on_bus(PAPR_I2C_PERIPH);
    s = wait_flag(I2C_FLAG_SBSEND);
    if (s != PAPR_OK) { return s; }

    i2c_master_addressing(PAPR_I2C_PERIPH, (uint32_t)(addr7 << 1),
                          I2C_RECEIVER);
    s = wait_flag(I2C_FLAG_ADDSEND);
    if (s != PAPR_OK) { return s; }
    i2c_flag_clear(PAPR_I2C_PERIPH, I2C_FLAG_ADDSEND);

    for (size_t i = 0U; i < len; ++i)
    {
        if (i + 1U == len)
        {
            i2c_ack_config(PAPR_I2C_PERIPH, I2C_ACK_DISABLE);
            i2c_stop_on_bus(PAPR_I2C_PERIPH);
        }
        s = wait_flag(I2C_FLAG_RBNE);
        if (s != PAPR_OK) { return s; }
        data[i] = (uint8_t)i2c_data_receive(PAPR_I2C_PERIPH);
    }

    /* Restore default ACK behaviour for subsequent transactions. */
    i2c_ack_config(PAPR_I2C_PERIPH, I2C_ACK_ENABLE);
    return PAPR_OK;
}

/* ------------------------------------------------------------------------- */
/* Buttons                                                                   */
/* ------------------------------------------------------------------------- */

bool papr_hal_button_power_pressed(void)
{
    return gpio_input_bit_get(PAPR_PIN_BTN_POWER_PORT,
                              PAPR_PIN_BTN_POWER_PIN) == RESET;
}

bool papr_hal_button_level_pressed(void)
{
    return gpio_input_bit_get(PAPR_PIN_BTN_LEVEL_PORT,
                              PAPR_PIN_BTN_LEVEL_PIN) == RESET;
}

papr_status_t papr_hal_led_set(uint8_t led_id, bool on)
{
    uint32_t port;
    uint32_t pin;
    switch (led_id)
    {
        case 0U: port = PAPR_PIN_LED_OK_PORT;    pin = PAPR_PIN_LED_OK_PIN;    break;
        case 1U: port = PAPR_PIN_LED_WARN_PORT;  pin = PAPR_PIN_LED_WARN_PIN;  break;
        case 2U: port = PAPR_PIN_LED_FAULT_PORT; pin = PAPR_PIN_LED_FAULT_PIN; break;
        default: return PAPR_ERR_PARAM;
    }
    if (on) { gpio_bit_set(port, pin); } else { gpio_bit_reset(port, pin); }
    return PAPR_OK;
}

/* ------------------------------------------------------------------------- */
/* Buzzer (TIMER2_CH0 PWM)                                                   */
/* ------------------------------------------------------------------------- */

static void buzzer_init(void)
{
    rcu_periph_clock_enable(RCU_TIMER2);
    gpio_init(PAPR_PIN_BUZZER_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ,
              PAPR_PIN_BUZZER_PIN);

    timer_deinit(PAPR_BUZZER_TIMER);
    timer_parameter_struct cfg;
    timer_struct_para_init(&cfg);
    cfg.prescaler        = (uint16_t)((SystemCoreClock / 1000000U) - 1U);
    cfg.alignedmode      = TIMER_COUNTER_EDGE;
    cfg.counterdirection = TIMER_COUNTER_UP;
    cfg.period           = 1000U;
    cfg.clockdivision    = TIMER_CKDIV_DIV1;
    timer_init(PAPR_BUZZER_TIMER, &cfg);

    timer_oc_parameter_struct oc;
    timer_channel_output_struct_para_init(&oc);
    oc.outputstate  = TIMER_CCX_ENABLE;
    oc.ocpolarity   = TIMER_OC_POLARITY_HIGH;
    oc.ocidlestate  = TIMER_OC_IDLE_STATE_LOW;
    timer_channel_output_config(PAPR_BUZZER_TIMER, PAPR_BUZZER_TIMER_CH, &oc);
    timer_channel_output_mode_config(PAPR_BUZZER_TIMER, PAPR_BUZZER_TIMER_CH,
                                     TIMER_OC_MODE_PWM0);
    timer_channel_output_pulse_value_config(PAPR_BUZZER_TIMER,
                                            PAPR_BUZZER_TIMER_CH, 0U);
    timer_primary_output_config(PAPR_BUZZER_TIMER, ENABLE);
    timer_enable(PAPR_BUZZER_TIMER);
}

papr_status_t papr_hal_buzzer_set(bool on, uint16_t freq_hz)
{
    if (!on || freq_hz == 0U)
    {
        timer_channel_output_pulse_value_config(PAPR_BUZZER_TIMER,
                                                PAPR_BUZZER_TIMER_CH, 0U);
        return PAPR_OK;
    }
    /* 1 MHz timebase from the prescaler in buzzer_init(). */
    uint32_t period = 1000000U / (uint32_t)freq_hz;
    if (period < 2U) { period = 2U; }
    timer_autoreload_value_config(PAPR_BUZZER_TIMER, (uint16_t)(period - 1U));
    timer_channel_output_pulse_value_config(PAPR_BUZZER_TIMER,
                                            PAPR_BUZZER_TIMER_CH,
                                            (uint16_t)(period / 2U));
    return PAPR_OK;
}

/* ------------------------------------------------------------------------- */
/* USART0 transport for the GD32VW553 BLE module                             */
/* ------------------------------------------------------------------------- */

#define BLE_RX_RING_LEN  256U
static volatile uint8_t  s_ble_rx_buf[BLE_RX_RING_LEN];
static volatile uint16_t s_ble_rx_head;     /* written by ISR */
static volatile uint16_t s_ble_rx_tail;     /* written by main */

static void ble_uart_init(void)
{
    rcu_periph_clock_enable(RCU_USART0);

    gpio_init(PAPR_PIN_UART_TX_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ,
              PAPR_PIN_UART_TX_PIN);
    gpio_init(PAPR_PIN_UART_RX_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ,
              PAPR_PIN_UART_RX_PIN);

    /* BLE module RESET_N: drive low at boot, release after init. */
    gpio_init(PAPR_PIN_BLE_RESET_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ,
              PAPR_PIN_BLE_RESET_PIN);
    gpio_bit_reset(PAPR_PIN_BLE_RESET_PORT, PAPR_PIN_BLE_RESET_PIN);

    /* HOST_WAKE input. */
    gpio_init(PAPR_PIN_BLE_WAKE_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ,
              PAPR_PIN_BLE_WAKE_PIN);

    usart_deinit(PAPR_UART_PERIPH);
    usart_baudrate_set(PAPR_UART_PERIPH, PAPR_BLE_UART_BAUD);
    usart_word_length_set(PAPR_UART_PERIPH, USART_WL_8BIT);
    usart_stop_bit_set(PAPR_UART_PERIPH, USART_STB_1BIT);
    usart_parity_config(PAPR_UART_PERIPH, USART_PM_NONE);
    usart_hardware_flow_rts_config(PAPR_UART_PERIPH, USART_RTS_DISABLE);
    usart_hardware_flow_cts_config(PAPR_UART_PERIPH, USART_CTS_DISABLE);
    usart_receive_config(PAPR_UART_PERIPH, USART_RECEIVE_ENABLE);
    usart_transmit_config(PAPR_UART_PERIPH, USART_TRANSMIT_ENABLE);
    usart_enable(PAPR_UART_PERIPH);

    /* RX-not-empty interrupt fills the ring buffer. */
    nvic_irq_enable(USART0_IRQn, 2U, 0U);
    usart_interrupt_enable(PAPR_UART_PERIPH, USART_INT_RBNE);

    s_ble_rx_head = 0U;
    s_ble_rx_tail = 0U;
}

void USART0_IRQHandler(void)
{
    if (usart_interrupt_flag_get(PAPR_UART_PERIPH, USART_INT_FLAG_RBNE) != RESET)
    {
        uint8_t b = (uint8_t)usart_data_receive(PAPR_UART_PERIPH);
        uint16_t next = (uint16_t)((s_ble_rx_head + 1U) % BLE_RX_RING_LEN);
        if (next != s_ble_rx_tail)
        {
            s_ble_rx_buf[s_ble_rx_head] = b;
            s_ble_rx_head = next;
        }
        /* If full, drop the byte. The protocol parser will resync on the
         * 0xAA 0x55 preamble of the next intact frame. */
    }
    /* Clear ORE if it has set so reception keeps running. */
    if (usart_flag_get(PAPR_UART_PERIPH, USART_FLAG_ORERR) != RESET)
    {
        usart_flag_clear(PAPR_UART_PERIPH, USART_FLAG_ORERR);
        (void)usart_data_receive(PAPR_UART_PERIPH);
    }
}

papr_status_t papr_hal_uart_write(const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0U) { return PAPR_ERR_PARAM; }
    for (size_t i = 0U; i < len; ++i)
    {
        uint32_t timeout = 100000U;
        while (usart_flag_get(PAPR_UART_PERIPH, USART_FLAG_TBE) == RESET &&
               --timeout) { }
        if (timeout == 0U) { return PAPR_ERR_TIMEOUT; }
        usart_data_transmit(PAPR_UART_PERIPH, (uint16_t)data[i]);
    }
    uint32_t timeout = 100000U;
    while (usart_flag_get(PAPR_UART_PERIPH, USART_FLAG_TC) == RESET &&
           --timeout) { }
    return (timeout == 0U) ? PAPR_ERR_TIMEOUT : PAPR_OK;
}

bool papr_hal_uart_read_byte(uint8_t *out)
{
    if (out == NULL || s_ble_rx_head == s_ble_rx_tail) { return false; }
    *out = s_ble_rx_buf[s_ble_rx_tail];
    s_ble_rx_tail = (uint16_t)((s_ble_rx_tail + 1U) % BLE_RX_RING_LEN);
    return true;
}

papr_status_t papr_hal_ble_set_reset(bool asserted)
{
    if (asserted) { gpio_bit_reset(PAPR_PIN_BLE_RESET_PORT, PAPR_PIN_BLE_RESET_PIN); }
    else          { gpio_bit_set(PAPR_PIN_BLE_RESET_PORT,   PAPR_PIN_BLE_RESET_PIN); }
    return PAPR_OK;
}

bool papr_hal_ble_host_wake(void)
{
    return gpio_input_bit_get(PAPR_PIN_BLE_WAKE_PORT,
                              PAPR_PIN_BLE_WAKE_PIN) != RESET;
}

/* ------------------------------------------------------------------------- */
/* Watchdog                                                                  */
/* ------------------------------------------------------------------------- */

static void wdt_init(void)
{
    rcu_osci_on(RCU_IRC40K);
    while (rcu_flag_get(RCU_FLAG_IRC40KSTB) == RESET) { /* spin */ }
    fwdgt_config(250U, FWDGT_PSC_DIV32); /* ≈ 200 ms timeout */
    fwdgt_enable();
}

void papr_hal_wdt_kick(void)
{
    fwdgt_counter_reload();
}

/* ------------------------------------------------------------------------- */
/* Top-level init                                                            */
/* ------------------------------------------------------------------------- */

papr_status_t papr_hal_init(void)
{
    s_tick_ms      = 0U;
    s_diag_pending = false;
    s_diag_edge_ms = 0U;
    s_tacho_rpm    = 0U;

    clock_init();
    gpio_clocks_enable();
    l6235_pins_init();
    ui_pins_init();
    dac_init_vref();
    tacho_timer_init();
    adc_init_papr();
    i2c_bus_init();
    buzzer_init();
    ble_uart_init();
    wdt_init();
    return PAPR_OK;
}

#endif /* GD32E51X */
