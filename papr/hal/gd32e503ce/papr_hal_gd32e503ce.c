/* HAL implementation for the GigaDevice GD32E503CE.
 *
 *   Core      : Cortex-M33 with FPU
 *   Package   : LQFP48 (~37 GPIOs)
 *   Flash/RAM : 256 KB / 64 KB
 *
 * This is an alternative MCU port: the application's HAL contract
 * (include/papr_hal.h) is identical to the GD32E517RE port, so papr_core
 * runs unchanged on either chip. Only the chip-specific peripheral setup,
 * pin assignments, and 256 KB flash map differ.
 *
 * The file targets the GigaDevice GD32E50x firmware library (gd32e50x.h
 * and the matching peripheral drivers). CMake includes it only when
 * PAPR_TARGET=GD32E503CE and pulls in startup_gd32e503_xb.s plus the
 * vendor SDK from GD32E50X_SDK_DIR. */

#ifdef GD32E50X

#include "gd32e50x.h"
#include "papr_config.h"
#include "papr_hal.h"
#include "papr_pinmap.h"
#include "papr_secure.h"
#include "papr_sha256.h"

#include <string.h>

/* ------------------------------------------------------------------------- */
/* Globals                                                                   */
/* ------------------------------------------------------------------------- */

static volatile uint32_t s_tick_ms;
static volatile bool     s_diag_pending;
static volatile uint32_t s_diag_edge_ms;
static volatile uint16_t s_tacho_rpm;

#define TACHO_SAMPLE_PERIOD_MS  100U

#define ADC_VREF_MV             3300U
#define ADC_FULL_SCALE          4095U

#define VBAT_DIV_NUM            220U
#define VBAT_DIV_DEN            43U

#define IBAT_MA_PER_COUNT_NUM   1U
#define IBAT_MA_PER_COUNT_DEN   3U

#define FLOW_FULL_SCALE_LPM     250U

/* ------------------------------------------------------------------------- */
/* Clock and SysTick                                                         */
/* ------------------------------------------------------------------------- */

static void clock_init(void)
{
    /* SystemInit() (vendor) sets the PLL from HXTAL. */
    SysTick_Config(SystemCoreClock / 1000U);
    NVIC_SetPriority(SysTick_IRQn, 0U);
}

void SysTick_Handler(void)
{
    ++s_tick_ms;

    static uint32_t s_last_sample_ms;
    static uint32_t s_last_count;
    if ((s_tick_ms - s_last_sample_ms) >= TACHO_SAMPLE_PERIOD_MS)
    {
        uint32_t count  = (uint32_t)timer_counter_read(PAPR_TACHO_TIMER);
        uint32_t delta  = (uint32_t)(count - s_last_count) & 0xFFFFU;
        s_last_count    = count;
        s_last_sample_ms = s_tick_ms;
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

    gpio_bit_reset(PAPR_PIN_L6235_EN_PORT,    PAPR_PIN_L6235_EN_PIN);
    gpio_bit_reset(PAPR_PIN_L6235_BRAKE_PORT, PAPR_PIN_L6235_BRAKE_PIN);
    gpio_bit_set(  PAPR_PIN_L6235_FWD_PORT,   PAPR_PIN_L6235_FWD_PIN);

    gpio_init(PAPR_PIN_L6235_DIAG_PORT, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ,
              PAPR_PIN_L6235_DIAG_PIN);
    gpio_exti_source_select(PAPR_PIN_L6235_DIAG_EXTI_PORT_SRC,
                            PAPR_PIN_L6235_DIAG_EXTI_SRC);
    exti_init(PAPR_PIN_L6235_DIAG_EXTI, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    exti_interrupt_flag_clear(PAPR_PIN_L6235_DIAG_EXTI);
    nvic_irq_enable(EXTI10_15_IRQn, 1U, 0U);

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
    if ((s_tick_ms - s_diag_edge_ms) < PAPR_L6235_DIAG_DEBOUNCE_MS) { return false; }
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
    rcu_adc_clock_config(RCU_CKADC_CKAPB2_DIV6);

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
    uint32_t code = adc_sample((uint8_t)PAPR_ADC_TEMP_CH);
    int32_t mv    = (int32_t)((code * ADC_VREF_MV) / ADC_FULL_SCALE);
    int32_t c10   = 250 + ((1500 - mv) / 18);
    if (c10 < -400) { c10 = -400; }
    if (c10 >  900) { c10 =  900; }
    *out = (int16_t)c10;
    return PAPR_OK;
}

/* ------------------------------------------------------------------------- */
/* I2C transport (SDP810 + GDY1124)                                          */
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
    i2c_master_addressing(PAPR_I2C_PERIPH, (uint32_t)(addr7 << 1), I2C_TRANSMITTER);
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
    i2c_master_addressing(PAPR_I2C_PERIPH, (uint32_t)(addr7 << 1), I2C_RECEIVER);
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
/* 3x3 switch matrix                                                         */
/* ------------------------------------------------------------------------- */

static const uint32_t s_keypad_row_ports[PAPR_KEYPAD_ROWS] = {
    PAPR_PIN_KEYPAD_ROW0_PORT, PAPR_PIN_KEYPAD_ROW1_PORT, PAPR_PIN_KEYPAD_ROW2_PORT
};
static const uint32_t s_keypad_row_pins[PAPR_KEYPAD_ROWS] = {
    PAPR_PIN_KEYPAD_ROW0_PIN,  PAPR_PIN_KEYPAD_ROW1_PIN,  PAPR_PIN_KEYPAD_ROW2_PIN
};
static const uint32_t s_keypad_col_ports[PAPR_KEYPAD_COLS] = {
    PAPR_PIN_KEYPAD_COL0_PORT, PAPR_PIN_KEYPAD_COL1_PORT, PAPR_PIN_KEYPAD_COL2_PORT
};
static const uint32_t s_keypad_col_pins[PAPR_KEYPAD_COLS] = {
    PAPR_PIN_KEYPAD_COL0_PIN,  PAPR_PIN_KEYPAD_COL1_PIN,  PAPR_PIN_KEYPAD_COL2_PIN
};

static void keypad_init(void)
{
    for (uint8_t r = 0U; r < PAPR_KEYPAD_ROWS; ++r)
    {
        gpio_init(s_keypad_row_ports[r], GPIO_MODE_OUT_OD, GPIO_OSPEED_2MHZ,
                  s_keypad_row_pins[r]);
        gpio_bit_set(s_keypad_row_ports[r], s_keypad_row_pins[r]);
    }
    for (uint8_t c = 0U; c < PAPR_KEYPAD_COLS; ++c)
    {
        gpio_init(s_keypad_col_ports[c], GPIO_MODE_IPU, GPIO_OSPEED_2MHZ,
                  s_keypad_col_pins[c]);
    }
}

papr_status_t papr_hal_keypad_drive_row(uint8_t row, bool active)
{
    if (row >= PAPR_KEYPAD_ROWS) { return PAPR_ERR_PARAM; }
    if (active) { gpio_bit_reset(s_keypad_row_ports[row], s_keypad_row_pins[row]); }
    else        { gpio_bit_set(s_keypad_row_ports[row],   s_keypad_row_pins[row]); }
    return PAPR_OK;
}

bool papr_hal_keypad_read_col(uint8_t col)
{
    if (col >= PAPR_KEYPAD_COLS) { return false; }
    return gpio_input_bit_get(s_keypad_col_ports[col],
                              s_keypad_col_pins[col]) == RESET;
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
static volatile uint16_t s_ble_rx_head;
static volatile uint16_t s_ble_rx_tail;

static void ble_uart_init(void)
{
    rcu_periph_clock_enable(RCU_USART0);

    gpio_init(PAPR_PIN_UART_TX_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ,
              PAPR_PIN_UART_TX_PIN);
    gpio_init(PAPR_PIN_UART_RX_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ,
              PAPR_PIN_UART_RX_PIN);

    gpio_init(PAPR_PIN_BLE_RESET_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ,
              PAPR_PIN_BLE_RESET_PIN);
    gpio_bit_reset(PAPR_PIN_BLE_RESET_PORT, PAPR_PIN_BLE_RESET_PIN);

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
    }
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
        while (usart_flag_get(PAPR_UART_PERIPH, USART_FLAG_TBE) == RESET && --timeout) { }
        if (timeout == 0U) { return PAPR_ERR_TIMEOUT; }
        usart_data_transmit(PAPR_UART_PERIPH, (uint16_t)data[i]);
    }
    uint32_t timeout = 100000U;
    while (usart_flag_get(PAPR_UART_PERIPH, USART_FLAG_TC) == RESET && --timeout) { }
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
/* OTA flash (FMC) — dual-bank application slots                             */
/* ------------------------------------------------------------------------- */
/*
 * 256 KB layout (see boot/boot_shared.h overrides in papr_pinmap.h):
 *   0x08000000  bootloader            ~28 KB
 *   0x08007000  boot-state page          2 KB (mutable)
 *   0x08007800  provisioning page        2 KB
 *   0x08008000  slot A (app)           112 KB  (last 1 KB = signed manifest)
 *   0x08024000  slot B (app)           112 KB  (last 1 KB = signed manifest)
 *   0x08040000  end
 */

static uint32_t ota_running_slot_base(void)
{
    uint32_t vtor = SCB->VTOR;
    return (vtor >= PAPR_SLOT_B_ADDR) ? PAPR_SLOT_B_ADDR : PAPR_SLOT_A_ADDR;
}

static uint32_t ota_staging_base(void)
{
    return (ota_running_slot_base() == PAPR_SLOT_A_ADDR)
            ? PAPR_SLOT_B_ADDR : PAPR_SLOT_A_ADDR;
}

uint32_t papr_hal_ota_slot_size(void)
{
    return PAPR_SLOT_APP_MAX;   /* minus the manifest page */
}

static papr_status_t fmc_program_buf(uint32_t addr, const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0U; i < len; i += 4U)
    {
        uint32_t word = 0xFFFFFFFFU;
        uint32_t take = ((len - i) >= 4U) ? 4U : (len - i);
        for (uint32_t b = 0U; b < take; ++b) { ((uint8_t *)&word)[b] = data[i + b]; }
        if (fmc_word_program(addr + i, word) != FMC_READY) { return PAPR_ERR_HW; }
    }
    return PAPR_OK;
}

papr_status_t papr_hal_ota_erase(void)
{
    uint32_t base = ota_staging_base();
    fmc_unlock();
    papr_status_t rc = PAPR_OK;
    for (uint32_t off = 0U; off < PAPR_SLOT_SIZE; off += PAPR_FLASH_PAGE)
    {
        if (fmc_page_erase(base + off) != FMC_READY) { rc = PAPR_ERR_HW; break; }
        papr_hal_wdt_kick();
    }
    fmc_lock();
    return rc;
}

papr_status_t papr_hal_ota_write(uint32_t offset, const uint8_t *data, uint32_t len)
{
    if (data == NULL) { return PAPR_ERR_PARAM; }
    fmc_unlock();
    papr_status_t rc = fmc_program_buf(ota_staging_base() + offset, data, len);
    fmc_lock();
    return rc;
}

papr_status_t papr_hal_ota_read(uint32_t offset, uint8_t *data, uint32_t len)
{
    if (data == NULL) { return PAPR_ERR_PARAM; }
    const uint8_t *src = (const uint8_t *)(ota_staging_base() + offset);
    for (uint32_t i = 0U; i < len; ++i) { data[i] = src[i]; }
    return PAPR_OK;
}

static uint8_t ota_staging_slot_index(void)
{
    return (ota_staging_base() == PAPR_SLOT_A_ADDR) ? 0U : 1U;
}
static uint8_t ota_running_slot_index(void)
{
    return (ota_running_slot_base() == PAPR_SLOT_A_ADDR) ? 0U : 1U;
}

static void boot_state_read(papr_boot_state_record_t *r)
{
    memcpy(r, (const void *)PAPR_BOOT_STATE_ADDR, sizeof(*r));
    if (r->magic != PAPR_IMG_MAGIC)
    {
        memset(r, 0, sizeof(*r));
        r->magic = PAPR_IMG_MAGIC;
        r->slot_state[0] = PAPR_BOOT_VALID;
        r->slot_state[1] = PAPR_BOOT_EMPTY;
    }
}

static papr_status_t boot_state_write(const papr_boot_state_record_t *r)
{
    fmc_unlock();
    papr_status_t rc = PAPR_OK;
    if (fmc_page_erase(PAPR_BOOT_STATE_ADDR) != FMC_READY) { rc = PAPR_ERR_HW; }
    if (rc == PAPR_OK)
    {
        rc = fmc_program_buf(PAPR_BOOT_STATE_ADDR, (const uint8_t *)r, sizeof(*r));
    }
    fmc_lock();
    return rc;
}

papr_status_t papr_hal_ota_commit(const papr_img_manifest_t *manifest)
{
    if (manifest == NULL) { return PAPR_ERR_PARAM; }

    uint32_t trailer = ota_staging_base() + PAPR_MANIFEST_OFFSET;
    fmc_unlock();
    papr_status_t rc = PAPR_OK;
    if (fmc_page_erase(trailer) != FMC_READY) { rc = PAPR_ERR_HW; }
    if (rc == PAPR_OK)
    {
        rc = fmc_program_buf(trailer, (const uint8_t *)manifest, sizeof(*manifest));
    }
    fmc_lock();
    if (rc != PAPR_OK) { return rc; }

    papr_boot_state_record_t st;
    boot_state_read(&st);
    uint8_t s = ota_staging_slot_index();
    st.slot_state[s]       = PAPR_BOOT_PENDING;
    st.slot_sec_version[s] = manifest->sec_version;
    st.try_count           = 0U;
    return boot_state_write(&st);
}

papr_status_t papr_hal_ota_confirm(void)
{
    papr_boot_state_record_t st;
    boot_state_read(&st);
    uint8_t r = ota_running_slot_index();
    if (st.slot_state[r] == PAPR_BOOT_VALID) { return PAPR_OK; }
    st.slot_state[r] = PAPR_BOOT_VALID;
    st.try_count     = 0U;
    return boot_state_write(&st);
}

void papr_hal_ota_reboot(void)
{
    __disable_irq();
    NVIC_SystemReset();
}

/* ------------------------------------------------------------------------- */
/* Production test / provisioning                                            */
/* ------------------------------------------------------------------------- */

#define GD32_UID_ADDR    0x1FFFF7E8U

bool papr_hal_factory_requested(void)
{
    rcu_periph_clock_enable(RCU_GPIOC);
    gpio_init(PAPR_PIN_TESTMODE_PORT, GPIO_MODE_IPU, GPIO_OSPEED_2MHZ,
              PAPR_PIN_TESTMODE_PIN);
    for (volatile uint32_t i = 0U; i < 10000U; ++i) { (void)i; }
    return gpio_input_bit_get(PAPR_PIN_TESTMODE_PORT,
                              PAPR_PIN_TESTMODE_PIN) == RESET;
}

void papr_hal_unique_id(uint8_t out[12])
{
    if (out == NULL) { return; }
    const volatile uint8_t *uid = (const volatile uint8_t *)GD32_UID_ADDR;
    for (uint8_t i = 0U; i < 12U; ++i) { out[i] = uid[i]; }
}

papr_status_t papr_hal_prov_read(uint8_t *data, uint32_t len)
{
    if (data == NULL) { return PAPR_ERR_PARAM; }
    const uint8_t *src = (const uint8_t *)PAPR_PROVISION_ADDR;
    for (uint32_t i = 0U; i < len; ++i) { data[i] = src[i]; }
    return PAPR_OK;
}

papr_status_t papr_hal_prov_write(const uint8_t *data, uint32_t len)
{
    if (data == NULL || len > PAPR_FLASH_PAGE) { return PAPR_ERR_PARAM; }
    fmc_unlock();
    papr_status_t rc = PAPR_OK;
    if (fmc_page_erase(PAPR_PROVISION_ADDR) != FMC_READY) { rc = PAPR_ERR_HW; }
    if (rc == PAPR_OK)
    {
        rc = fmc_program_buf(PAPR_PROVISION_ADDR, data, len);
    }
    fmc_lock();
    return rc;
}

/* ------------------------------------------------------------------------- */
/* Security services                                                         */
/* ------------------------------------------------------------------------- */

#define SEC_KEY_SESSION_ADDR  (PAPR_PROVISION_ADDR + 0x100U)
#define SEC_KEY_VENDOR_ADDR   (PAPR_PROVISION_ADDR + 0x140U)

papr_status_t papr_hal_sec_key_read(uint8_t key_id, uint8_t *out, uint32_t len)
{
    if (out == NULL || len > PAPR_SEC_KEY_LEN) { return PAPR_ERR_PARAM; }
    uint32_t addr = (key_id == PAPR_KEY_SESSION) ? SEC_KEY_SESSION_ADDR
                  : (key_id == PAPR_KEY_VENDOR)  ? SEC_KEY_VENDOR_ADDR : 0U;
    if (addr == 0U) { return PAPR_ERR_PARAM; }
    const uint8_t *src = (const uint8_t *)addr;
    uint8_t acc = 0xFFU;
    for (uint32_t i = 0U; i < len; ++i) { out[i] = src[i]; acc &= src[i]; }
    return (acc == 0xFFU) ? PAPR_ERR_NOT_READY : PAPR_OK;
}

bool papr_hal_sec_verify(const uint8_t digest[32], const uint8_t *sig, uint32_t sig_len)
{
    if (digest == NULL || sig == NULL || sig_len < 32U) { return false; }
    uint8_t vkey[PAPR_SEC_KEY_LEN];
    if (papr_hal_sec_key_read(PAPR_KEY_VENDOR, vkey, sizeof(vkey)) != PAPR_OK)
    {
        return false;
    }
    uint8_t tag[PAPR_SHA256_DIGEST_LEN];
    papr_hmac_sha256(vkey, sizeof(vkey), digest, 32U, tag);
    memset(vkey, 0, sizeof(vkey));
    return papr_ct_equal(tag, sig, PAPR_SHA256_DIGEST_LEN);
}

papr_status_t papr_hal_rng(uint8_t *out, uint32_t len)
{
    if (out == NULL) { return PAPR_ERR_PARAM; }
    static uint32_t ctr = 0U;
    while (len > 0U)
    {
        uint8_t seed[12 + 4 + 4];
        papr_hal_unique_id(seed);
        uint32_t t = (uint32_t)SysTick->VAL ^ s_tick_ms;
        memcpy(&seed[12], &t, 4);
        memcpy(&seed[16], &ctr, 4);
        ctr++;
        uint8_t block[PAPR_SHA256_DIGEST_LEN];
        papr_sha256(seed, sizeof(seed), block);
        uint32_t n = (len < sizeof(block)) ? len : (uint32_t)sizeof(block);
        memcpy(out, block, n);
        out += n;
        len -= n;
    }
    return PAPR_OK;
}

uint32_t papr_hal_sec_version_get(void)
{
    papr_boot_state_record_t st;
    boot_state_read(&st);
    return st.reserved;
}

papr_status_t papr_hal_sec_version_set(uint32_t version)
{
    papr_boot_state_record_t st;
    boot_state_read(&st);
    if (version <= st.reserved) { return PAPR_OK; }
    st.reserved = version;
    return boot_state_write(&st);
}

papr_status_t papr_hal_secure_lock_debug(void)
{
    fmc_unlock();
    ob_unlock();
    ob_security_protection_config(OB_LSPC);
    ob_lock();
    fmc_lock();
    return PAPR_OK;
}

bool papr_hal_debug_locked(void)
{
    return ob_spc_get() != FMC_NSPC;
}

/* ------------------------------------------------------------------------- */
/* Watchdog                                                                  */
/* ------------------------------------------------------------------------- */

static void wdt_init(void)
{
    rcu_osci_on(RCU_IRC40K);
    while (rcu_flag_get(RCU_FLAG_IRC40KSTB) == RESET) { /* spin */ }
    fwdgt_config(250U, FWDGT_PSC_DIV32);
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
    keypad_init();
    buzzer_init();
    ble_uart_init();
    wdt_init();
    return PAPR_OK;
}

#endif /* GD32E50X */
