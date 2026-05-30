#ifndef PAPR_PINMAP_GD32E517RE_H
#define PAPR_PINMAP_GD32E517RE_H

/* Pin map for the GigaDevice GD32E517RE (Cortex-M33, LQFP64, 51 GPIOs).
 *
 * Reserved pins (not available as GPIO on this package):
 *   VDD/VSS                     pwr
 *   VDDA/VSSA                   analog pwr
 *   VBAT                        backup pwr
 *   NRST                        reset
 *   PA13 / PA14                 SWDIO / SWCLK
 *   PD0 / PD1                   HXTAL (25 MHz crystal)
 *   PC14 / PC15                 LXTAL (32.768 kHz crystal)
 *   BOOT0                       boot select
 *
 * Everything below is wired to a peripheral. Pin numbers are LQFP64 package
 * positions; refer to the GD32E517 datasheet pinout for exact mapping. */

#include "gd32e51x.h"

/* ---- L6235 brushless DC driver -------------------------------------------- */

/* DAC channel 0 → L6235 VREF (peak-current setpoint).                       */
#define PAPR_PIN_L6235_VREF_PORT     GPIOA
#define PAPR_PIN_L6235_VREF_PIN      GPIO_PIN_4
#define PAPR_DAC_CHANNEL             DAC_OUT0

/* GPIO outputs to L6235 logic inputs.                                       */
#define PAPR_PIN_L6235_EN_PORT       GPIOB
#define PAPR_PIN_L6235_EN_PIN        GPIO_PIN_12

#define PAPR_PIN_L6235_FWD_PORT      GPIOB
#define PAPR_PIN_L6235_FWD_PIN       GPIO_PIN_13

#define PAPR_PIN_L6235_BRAKE_PORT    GPIOB
#define PAPR_PIN_L6235_BRAKE_PIN     GPIO_PIN_14

/* DIAG: open-drain from L6235 with external pull-up; EXTI on falling edge.  */
#define PAPR_PIN_L6235_DIAG_PORT     GPIOB
#define PAPR_PIN_L6235_DIAG_PIN      GPIO_PIN_15
#define PAPR_PIN_L6235_DIAG_EXTI     EXTI_15
#define PAPR_PIN_L6235_DIAG_EXTI_SRC EXTI_SOURCE_PIN15
#define PAPR_PIN_L6235_DIAG_EXTI_PORT_SRC EXTI_SOURCE_GPIOB

/* TACHO: open-drain from L6235; counted by TIMER1 in external clock mode.   */
#define PAPR_PIN_L6235_TACHO_PORT    GPIOA
#define PAPR_PIN_L6235_TACHO_PIN     GPIO_PIN_8
#define PAPR_PIN_L6235_TACHO_AF      GPIO_AF_1
#define PAPR_TACHO_TIMER             TIMER1

/* ---- Battery, flow, pressure, temperature -------------------------------- */

/* ADC0 multi-channel scan: pack voltage divider, shunt amplifier, flow,    */
/* and NTC thermistor on the motor housing.                                  */
#define PAPR_PIN_VBAT_PORT           GPIOA
#define PAPR_PIN_VBAT_PIN            GPIO_PIN_0
#define PAPR_ADC_VBAT_CH             ADC_CHANNEL_0

#define PAPR_PIN_IBAT_PORT           GPIOA
#define PAPR_PIN_IBAT_PIN            GPIO_PIN_1
#define PAPR_ADC_IBAT_CH             ADC_CHANNEL_1

#define PAPR_PIN_FLOW_PORT           GPIOA
#define PAPR_PIN_FLOW_PIN            GPIO_PIN_2
#define PAPR_ADC_FLOW_CH             ADC_CHANNEL_2

#define PAPR_PIN_TEMP_PORT           GPIOA
#define PAPR_PIN_TEMP_PIN            GPIO_PIN_3
#define PAPR_ADC_TEMP_CH             ADC_CHANNEL_3

/* I2C0 to differential-pressure sensor (e.g. Sensirion SDP610 or NXP MPXV). */
#define PAPR_PIN_I2C_SCL_PORT        GPIOB
#define PAPR_PIN_I2C_SCL_PIN         GPIO_PIN_6
#define PAPR_PIN_I2C_SDA_PORT        GPIOB
#define PAPR_PIN_I2C_SDA_PIN         GPIO_PIN_7
#define PAPR_I2C_PERIPH              I2C0

/* ---- User interface ------------------------------------------------------ */

#define PAPR_PIN_BTN_POWER_PORT      GPIOC
#define PAPR_PIN_BTN_POWER_PIN       GPIO_PIN_13

#define PAPR_PIN_BTN_LEVEL_PORT      GPIOB
#define PAPR_PIN_BTN_LEVEL_PIN       GPIO_PIN_0

#define PAPR_PIN_LED_OK_PORT         GPIOB
#define PAPR_PIN_LED_OK_PIN          GPIO_PIN_1

#define PAPR_PIN_LED_WARN_PORT       GPIOB
#define PAPR_PIN_LED_WARN_PIN        GPIO_PIN_2

#define PAPR_PIN_LED_FAULT_PORT      GPIOB
#define PAPR_PIN_LED_FAULT_PIN       GPIO_PIN_10

/* Buzzer driven by TIMER2_CH0 PWM. */
#define PAPR_PIN_BUZZER_PORT         GPIOA
#define PAPR_PIN_BUZZER_PIN          GPIO_PIN_6
#define PAPR_PIN_BUZZER_AF           GPIO_AF_2
#define PAPR_BUZZER_TIMER            TIMER2
#define PAPR_BUZZER_TIMER_CH         TIMER_CH_0

/* ---- GD32VW553-UNIFI-EMH7 BLE module ------------------------------------- */

/* USART0 carries the framed binary protocol to and from the BLE module. */
#define PAPR_PIN_UART_TX_PORT        GPIOA
#define PAPR_PIN_UART_TX_PIN         GPIO_PIN_9
#define PAPR_PIN_UART_RX_PORT        GPIOA
#define PAPR_PIN_UART_RX_PIN         GPIO_PIN_10
#define PAPR_UART_PERIPH             USART0

/* Module reset (active LOW). Pulled high externally; MCU drives low to
 * reboot the radio. */
#define PAPR_PIN_BLE_RESET_PORT      GPIOA
#define PAPR_PIN_BLE_RESET_PIN       GPIO_PIN_11

/* Host-wake input from the module (high when the module has data to deliver
 * outside of the normal UART RX path; informational here). */
#define PAPR_PIN_BLE_WAKE_PORT       GPIOA
#define PAPR_PIN_BLE_WAKE_PIN        GPIO_PIN_12

/* GDY1124 absolute pressure sensor shares I2C0 with the SDP810
 * (different addresses). No new pins required. */

/* ---- 3x3 switch matrix --------------------------------------------------
 * Six previously-unused GPIOs scan the user keypad. Rows are open-drain
 * outputs driven low to activate; columns are inputs with internal pull-up,
 * so a pressed key reads low through the closed switch. */

#define PAPR_PIN_KEYPAD_ROW0_PORT    GPIOB
#define PAPR_PIN_KEYPAD_ROW0_PIN     GPIO_PIN_3
#define PAPR_PIN_KEYPAD_ROW1_PORT    GPIOB
#define PAPR_PIN_KEYPAD_ROW1_PIN     GPIO_PIN_4
#define PAPR_PIN_KEYPAD_ROW2_PORT    GPIOB
#define PAPR_PIN_KEYPAD_ROW2_PIN     GPIO_PIN_5

#define PAPR_PIN_KEYPAD_COL0_PORT    GPIOB
#define PAPR_PIN_KEYPAD_COL0_PIN     GPIO_PIN_8
#define PAPR_PIN_KEYPAD_COL1_PORT    GPIOB
#define PAPR_PIN_KEYPAD_COL1_PIN     GPIO_PIN_9
#define PAPR_PIN_KEYPAD_COL2_PORT    GPIOB
#define PAPR_PIN_KEYPAD_COL2_PIN     GPIO_PIN_11

/* ---- Production test (DFT) ----------------------------------------------
 * TEST_MODE pad: a bed-of-nails / pogo test point the end-of-line fixture
 * pulls LOW to make the firmware boot into the factory command loop. Idle-
 * high via the internal pull-up so the field unit always boots the app.
 * PC12 is brought to a dedicated test pad next to the SWD header. */
#define PAPR_PIN_TESTMODE_PORT       GPIOC
#define PAPR_PIN_TESTMODE_PIN        GPIO_PIN_12

/* The functional-test transport reuses the BLE UART (PA9/PA10) on test pads;
 * SWD (PA13/PA14) is the flashing interface. Both are documented in
 * ARCHITECTURE.md "Production / test access". */

#endif /* PAPR_PINMAP_GD32E517RE_H */
