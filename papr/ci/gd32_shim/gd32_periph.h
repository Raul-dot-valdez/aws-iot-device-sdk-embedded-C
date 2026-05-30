/* Permissive GigaDevice GD32 SDK shim for CI syntax-checking ONLY.
 *
 * Purpose
 * -------
 * The two GD32 HAL ports (hal/gd32e517re, hal/gd32e503ce) are gated on
 * GD32E51X / GD32E50X and reference vendor headers / functions that no host
 * CI runner has by default. To catch the most common breakage class —
 * refactoring the papr_hal contract and forgetting to update one of the
 * MCU ports — CI compiles each port with
 *
 *     arm-none-eabi-gcc -fsyntax-only -I ci/gd32_shim ...
 *
 * (or even plain gcc on a non-cross host; the shim is portable C).
 *
 * This file is a permissive surface, not a model of the real silicon:
 *   - it declares (does not define) every SDK function the HAL calls
 *   - it provides every constant / typedef / macro the HAL references
 *   - it is INTENTIONALLY NOT a working SDK — never ship a unit built
 *     against this; use the real GigaDevice firmware library.
 */

#ifndef PAPR_CI_GD32_SHIM_H
#define PAPR_CI_GD32_SHIM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ---- CMSIS-ish core peripherals + intrinsics ----------------------------- */

typedef struct { volatile uint32_t VTOR; } scb_t;
typedef struct { volatile uint32_t VAL;  } systick_t;
extern scb_t     *const SCB;
extern systick_t *const SysTick;

/* IRQ numbers used by the HAL. */
typedef int IRQn_Type;
#define SysTick_IRQn       (-1)
#define EXTI10_15_IRQn     1
#define USART0_IRQn        2

static inline void NVIC_SetPriority(IRQn_Type irq, uint32_t prio) { (void)irq; (void)prio; }
static inline void NVIC_SystemReset(void) { for (;;) {} }
static inline void __disable_irq(void) {}
static inline void __DSB(void)         {}
static inline void __ISB(void)         {}
static inline void __set_MSP(uint32_t v) { (void)v; }
static inline int  SysTick_Config(uint32_t ticks) { (void)ticks; return 0; }

/* ---- generic SDK status enums ------------------------------------------- */

typedef enum { RESET = 0, SET = 1 } FlagStatus;
typedef int ErrStatus;
#define DISABLE 0
#define ENABLE  1

/* fmc_state_enum */
typedef int fmc_state_enum;
#define FMC_READY  0
#define FMC_NSPC   0     /* "no security protection" */

/* ---- bus / clock control (RCU) ------------------------------------------ */

typedef int rcu_periph_enum;
typedef int rcu_periph_flag_enum;
typedef int rcu_osci_type_enum;
typedef int rcu_adc_clock_enum;

#define RCU_GPIOA  1
#define RCU_GPIOB  2
#define RCU_GPIOC  3
#define RCU_AF     4
#define RCU_DAC    5
#define RCU_ADC0   6
#define RCU_I2C0   7
#define RCU_USART0 8
#define RCU_TIMER1 9
#define RCU_TIMER2 10
#define RCU_IRC40K 11

#define RCU_FLAG_IRC40KSTB 1
#define RCU_CKADC_CKAPB2_DIV6 1

void rcu_periph_clock_enable(rcu_periph_enum p);
void rcu_adc_clock_config(rcu_adc_clock_enum c);
void rcu_osci_on(rcu_osci_type_enum o);
FlagStatus rcu_flag_get(rcu_periph_flag_enum f);

/* ---- GPIO --------------------------------------------------------------- */

#define GPIOA 0x40010800U
#define GPIOB 0x40010C00U
#define GPIOC 0x40011000U

#define GPIO_PIN_0  0x0001U
#define GPIO_PIN_1  0x0002U
#define GPIO_PIN_2  0x0004U
#define GPIO_PIN_3  0x0008U
#define GPIO_PIN_4  0x0010U
#define GPIO_PIN_5  0x0020U
#define GPIO_PIN_6  0x0040U
#define GPIO_PIN_7  0x0080U
#define GPIO_PIN_8  0x0100U
#define GPIO_PIN_9  0x0200U
#define GPIO_PIN_10 0x0400U
#define GPIO_PIN_11 0x0800U
#define GPIO_PIN_12 0x1000U
#define GPIO_PIN_13 0x2000U
#define GPIO_PIN_14 0x4000U
#define GPIO_PIN_15 0x8000U

#define GPIO_MODE_OUT_PP      1
#define GPIO_MODE_OUT_OD      2
#define GPIO_MODE_IPU         3
#define GPIO_MODE_AF_PP       4
#define GPIO_MODE_AF_OD       5
#define GPIO_MODE_AIN         6
#define GPIO_MODE_IN_FLOATING 7

#define GPIO_OSPEED_2MHZ  2
#define GPIO_OSPEED_50MHZ 50

#define GPIO_AF_1 1
#define GPIO_AF_2 2

void gpio_init(uint32_t port, uint32_t mode, uint32_t speed, uint32_t pin);
void gpio_bit_set(uint32_t port, uint32_t pin);
void gpio_bit_reset(uint32_t port, uint32_t pin);
FlagStatus gpio_input_bit_get(uint32_t port, uint32_t pin);
void gpio_exti_source_select(uint8_t port_src, uint8_t pin_src);

/* ---- EXTI --------------------------------------------------------------- */

typedef int exti_line_enum;
typedef int exti_mode_enum;
typedef int exti_trig_type_enum;

#define EXTI_15 15
#define EXTI_INTERRUPT 0
#define EXTI_TRIG_FALLING 1
#define EXTI_SOURCE_PIN15  15
#define EXTI_SOURCE_GPIOA  0
#define EXTI_SOURCE_GPIOB  1
#define EXTI_SOURCE_GPIOC  2

void exti_init(exti_line_enum line, exti_mode_enum mode, exti_trig_type_enum trig);
void exti_interrupt_flag_clear(exti_line_enum line);
FlagStatus exti_interrupt_flag_get(exti_line_enum line);

void nvic_irq_enable(IRQn_Type irq, uint8_t pre, uint8_t sub);

/* ---- DAC ---------------------------------------------------------------- */

#define DAC_OUT0 0
#define DAC_ALIGN_12B_R 0
#define DAC_WAVE_DISABLE 0

void dac_deinit(void);
void dac_trigger_disable(uint32_t ch);
void dac_wave_mode_config(uint32_t ch, uint32_t mode);
void dac_output_buffer_enable(uint32_t ch);
void dac_data_set(uint32_t ch, uint32_t align, uint16_t code);
void dac_enable(uint32_t ch);

/* ---- ADC ---------------------------------------------------------------- */

#define ADC0 0x40012400U
#define ADC_MODE_FREE 0
#define ADC_DATAALIGN_RIGHT 0
#define ADC_RESOLUTION_12B 0
#define ADC_REGULAR_CHANNEL 0
#define ADC0_1_EXTTRIG_REGULAR_NONE 0
#define ADC_CONTINUOUS_MODE 0
#define ADC_SCAN_MODE 1
#define ADC_FLAG_EOC 1
#define ADC_CHANNEL_0 0
#define ADC_CHANNEL_1 1
#define ADC_CHANNEL_2 2
#define ADC_CHANNEL_3 3
#define ADC_SAMPLETIME_239POINT5 0

void adc_deinit(uint32_t adc);
void adc_mode_config(uint32_t mode);
void adc_data_alignment_config(uint32_t adc, uint32_t align);
void adc_resolution_config(uint32_t adc, uint32_t res);
void adc_external_trigger_source_config(uint32_t adc, uint32_t ch, uint32_t src);
void adc_external_trigger_config(uint32_t adc, uint32_t ch, uint32_t en);
void adc_special_function_config(uint32_t adc, uint32_t func, uint32_t en);
void adc_enable(uint32_t adc);
void adc_calibration_enable(uint32_t adc);
void adc_regular_channel_config(uint32_t adc, uint8_t rank, uint8_t ch, uint32_t sampletime);
void adc_software_trigger_enable(uint32_t adc, uint32_t ch);
FlagStatus adc_flag_get(uint32_t adc, uint32_t flag);
void adc_flag_clear(uint32_t adc, uint32_t flag);
uint16_t adc_regular_data_read(uint32_t adc);

/* ---- I2C ---------------------------------------------------------------- */

#define I2C0 0x40005400U
#define I2C_DTCY_2 0
#define I2C_I2CMODE_ENABLE 1
#define I2C_ADDFORMAT_7BITS 0
#define I2C_ACK_ENABLE 1
#define I2C_ACK_DISABLE 0
#define I2C_ACKPOS_CURRENT 0
#define I2C_TRANSMITTER 0
#define I2C_RECEIVER 1
#define I2C_FLAG_SBSEND 1
#define I2C_FLAG_ADDSEND 2
#define I2C_FLAG_TBE 3
#define I2C_FLAG_BTC 4
#define I2C_FLAG_RBNE 5
#define I2C_FLAG_I2CBSY 6

void i2c_deinit(uint32_t i2c);
void i2c_clock_config(uint32_t i2c, uint32_t clk, uint32_t dtcy);
void i2c_mode_addr_config(uint32_t i2c, uint32_t mode, uint32_t fmt, uint32_t addr);
void i2c_enable(uint32_t i2c);
void i2c_ack_config(uint32_t i2c, uint32_t en);
void i2c_ackpos_config(uint32_t i2c, uint32_t pos);
void i2c_start_on_bus(uint32_t i2c);
void i2c_stop_on_bus(uint32_t i2c);
void i2c_master_addressing(uint32_t i2c, uint32_t addr, uint32_t dir);
void i2c_data_transmit(uint32_t i2c, uint8_t data);
uint8_t i2c_data_receive(uint32_t i2c);
FlagStatus i2c_flag_get(uint32_t i2c, uint32_t flag);
void i2c_flag_clear(uint32_t i2c, uint32_t flag);

/* ---- USART -------------------------------------------------------------- */

#define USART0 0x40013800U
#define USART_WL_8BIT 0
#define USART_STB_1BIT 0
#define USART_PM_NONE 0
#define USART_RTS_DISABLE 0
#define USART_CTS_DISABLE 0
#define USART_RECEIVE_ENABLE 1
#define USART_TRANSMIT_ENABLE 1
#define USART_INT_RBNE 1
#define USART_INT_FLAG_RBNE 1
#define USART_FLAG_TBE 1
#define USART_FLAG_TC  2
#define USART_FLAG_ORERR 3

void usart_deinit(uint32_t usart);
void usart_baudrate_set(uint32_t usart, uint32_t baud);
void usart_word_length_set(uint32_t usart, uint32_t wl);
void usart_stop_bit_set(uint32_t usart, uint32_t stb);
void usart_parity_config(uint32_t usart, uint32_t pm);
void usart_hardware_flow_rts_config(uint32_t usart, uint32_t en);
void usart_hardware_flow_cts_config(uint32_t usart, uint32_t en);
void usart_receive_config(uint32_t usart, uint32_t en);
void usart_transmit_config(uint32_t usart, uint32_t en);
void usart_enable(uint32_t usart);
void usart_interrupt_enable(uint32_t usart, uint32_t intr);
FlagStatus usart_interrupt_flag_get(uint32_t usart, uint32_t flag);
uint16_t usart_data_receive(uint32_t usart);
void usart_data_transmit(uint32_t usart, uint16_t data);
FlagStatus usart_flag_get(uint32_t usart, uint32_t flag);
void usart_flag_clear(uint32_t usart, uint32_t flag);

/* ---- TIMER -------------------------------------------------------------- */

#define TIMER1 0x40000400U
#define TIMER2 0x40000800U

#define TIMER_COUNTER_EDGE 0
#define TIMER_COUNTER_UP   0
#define TIMER_CKDIV_DIV1   0
#define TIMER_SLAVE_MODE_EXTERNAL0 0
#define TIMER_SMCFG_TRGSEL_CI0FE0 0
#define TIMER_IC_POLARITY_RISING 0
#define TIMER_IC_SELECTION_DIRECTTI 0
#define TIMER_IC_PSC_DIV1 0
#define TIMER_CH_0 0
#define TIMER_CCX_ENABLE 1
#define TIMER_OC_POLARITY_HIGH 0
#define TIMER_OC_IDLE_STATE_LOW 0
#define TIMER_OC_MODE_PWM0 0

typedef struct {
    uint16_t prescaler;
    uint16_t alignedmode;
    uint16_t counterdirection;
    uint32_t period;
    uint16_t clockdivision;
    uint8_t  repetitioncounter;
} timer_parameter_struct;

typedef struct {
    uint16_t icpolarity;
    uint16_t icselection;
    uint16_t icprescaler;
    uint8_t  icfilter;
} timer_input_capture_struct;

typedef struct {
    uint16_t outputstate;
    uint16_t outputnstate;
    uint16_t ocpolarity;
    uint16_t ocnpolarity;
    uint16_t ocidlestate;
    uint16_t ocnidlestate;
} timer_oc_parameter_struct;

void timer_deinit(uint32_t tim);
void timer_struct_para_init(timer_parameter_struct *p);
void timer_init(uint32_t tim, timer_parameter_struct *p);
void timer_slave_mode_select(uint32_t tim, uint32_t mode);
void timer_input_trigger_source_select(uint32_t tim, uint32_t src);
void timer_input_capture_config(uint32_t tim, uint16_t ch, timer_input_capture_struct *ic);
void timer_enable(uint32_t tim);
uint32_t timer_counter_read(uint32_t tim);
void timer_channel_output_struct_para_init(timer_oc_parameter_struct *p);
void timer_channel_output_config(uint32_t tim, uint16_t ch, timer_oc_parameter_struct *p);
void timer_channel_output_mode_config(uint32_t tim, uint16_t ch, uint16_t mode);
void timer_channel_output_pulse_value_config(uint32_t tim, uint16_t ch, uint32_t pulse);
void timer_primary_output_config(uint32_t tim, uint32_t en);
void timer_autoreload_value_config(uint32_t tim, uint16_t arr);

/* ---- FMC + OB (flash) --------------------------------------------------- */

#define OB_LSPC 1

void fmc_unlock(void);
void fmc_lock(void);
fmc_state_enum fmc_page_erase(uint32_t addr);
fmc_state_enum fmc_word_program(uint32_t addr, uint32_t word);
void ob_unlock(void);
void ob_lock(void);
void ob_security_protection_config(uint32_t level);
uint8_t ob_spc_get(void);

/* ---- watchdog ----------------------------------------------------------- */

#define FWDGT_PSC_DIV32 0
void fwdgt_config(uint32_t reload, uint32_t prescaler);
void fwdgt_enable(void);
void fwdgt_counter_reload(void);

/* ---- misc --------------------------------------------------------------- */

void delay_1ms(uint32_t ms);
extern uint32_t SystemCoreClock;
void SystemInit(void);

#endif /* PAPR_CI_GD32_SHIM_H */
