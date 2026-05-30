#ifndef PAPR_PINMAP_GD32E503CE_H
#define PAPR_PINMAP_GD32E503CE_H

/* Pin map for the GigaDevice GD32E503CE (Cortex-M33, LQFP48, ~37 GPIOs,
 * 256 KB flash, 64 KB SRAM). Alternative MCU choice for cost / area-
 * constrained PAPR builds. The HAL contract is identical to the
 * GD32E517RE port; only the chip-specific addresses and pin assignments
 * differ.
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
 */

#include "gd32e50x.h"

/* ---- Flash map override (256 KB) ----------------------------------------
 * Two app slots of 112 KB each fit alongside the 32 KB bootloader / state /
 * provisioning region. See boot/boot_shared.h for the meaning of each
 * symbol; these defines override the GD32E517RE defaults. They go through
 * #undef so the override is explicit no matter what include order pulled in
 * boot_shared.h first. */
#undef  PAPR_FLASH_PAGE
#define PAPR_FLASH_PAGE          1024U   /* GD32E50x small bank: 1 KB pages   */
#undef  PAPR_SLOT_SIZE
#define PAPR_SLOT_SIZE           (112U * 1024U)
#undef  PAPR_SLOT_A_ADDR
#define PAPR_SLOT_A_ADDR         0x08008000U
#undef  PAPR_SLOT_B_ADDR
#define PAPR_SLOT_B_ADDR         0x08024000U
/* PAPR_SLOT_APP_MAX and PAPR_MANIFEST_OFFSET in boot_shared.h are computed
 * from the macros above; redefine them too so they pick up the new size. */
#undef  PAPR_SLOT_APP_MAX
#define PAPR_SLOT_APP_MAX        (PAPR_SLOT_SIZE - PAPR_FLASH_PAGE)
#undef  PAPR_MANIFEST_OFFSET
#define PAPR_MANIFEST_OFFSET     (PAPR_SLOT_SIZE - PAPR_FLASH_PAGE)

/* ---- L6235 brushless DC driver ------------------------------------------- */

/* DAC channel 0 -> L6235 VREF (peak-current setpoint).                      */
#define PAPR_PIN_L6235_VREF_PORT     GPIOA
#define PAPR_PIN_L6235_VREF_PIN      GPIO_PIN_4
#define PAPR_DAC_CHANNEL             DAC_OUT0

/* GPIO outputs to L6235 logic inputs (kept on port B same as GD32E517RE).    */
#define PAPR_PIN_L6235_EN_PORT       GPIOB
#define PAPR_PIN_L6235_EN_PIN        GPIO_PIN_12

#define PAPR_PIN_L6235_FWD_PORT      GPIOB
#define PAPR_PIN_L6235_FWD_PIN       GPIO_PIN_13

#define PAPR_PIN_L6235_BRAKE_PORT    GPIOB
#define PAPR_PIN_L6235_BRAKE_PIN     GPIO_PIN_14

/* DIAG: open-drain, external pull-up; EXTI on falling edge.                  */
#define PAPR_PIN_L6235_DIAG_PORT     GPIOB
#define PAPR_PIN_L6235_DIAG_PIN      GPIO_PIN_15
#define PAPR_PIN_L6235_DIAG_EXTI     EXTI_15
#define PAPR_PIN_L6235_DIAG_EXTI_SRC EXTI_SOURCE_PIN15
#define PAPR_PIN_L6235_DIAG_EXTI_PORT_SRC EXTI_SOURCE_GPIOB

/* TACHO: open-drain; counted by TIMER1 in external clock mode.              */
#define PAPR_PIN_L6235_TACHO_PORT    GPIOA
#define PAPR_PIN_L6235_TACHO_PIN     GPIO_PIN_8
#define PAPR_PIN_L6235_TACHO_AF      GPIO_AF_1
#define PAPR_TACHO_TIMER             TIMER1

/* ---- Battery, flow, pressure, temperature -------------------------------- */

/* ADC0 multi-channel scan: VBAT, IBAT, FLOW, NTC.                           */
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

/* I2C0 to differential-pressure (SDP810) + absolute-pressure (GDY1124).     */
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

/* LQFP48: PB10 may not be available on every variant. Moved LED_FAULT to PA5
 * (a free pin on this package) to keep all three LEDs usable. */
#define PAPR_PIN_LED_FAULT_PORT      GPIOA
#define PAPR_PIN_LED_FAULT_PIN       GPIO_PIN_5

/* Buzzer driven by TIMER2_CH0 PWM. */
#define PAPR_PIN_BUZZER_PORT         GPIOA
#define PAPR_PIN_BUZZER_PIN          GPIO_PIN_6
#define PAPR_PIN_BUZZER_AF           GPIO_AF_2
#define PAPR_BUZZER_TIMER            TIMER2
#define PAPR_BUZZER_TIMER_CH         TIMER_CH_0

/* ---- GD32VW553-UNIFI-EMH7 BLE module ------------------------------------- */

#define PAPR_PIN_UART_TX_PORT        GPIOA
#define PAPR_PIN_UART_TX_PIN         GPIO_PIN_9
#define PAPR_PIN_UART_RX_PORT        GPIOA
#define PAPR_PIN_UART_RX_PIN         GPIO_PIN_10
#define PAPR_UART_PERIPH             USART0

#define PAPR_PIN_BLE_RESET_PORT      GPIOA
#define PAPR_PIN_BLE_RESET_PIN       GPIO_PIN_11

#define PAPR_PIN_BLE_WAKE_PORT       GPIOA
#define PAPR_PIN_BLE_WAKE_PIN        GPIO_PIN_12

/* ---- 3x3 switch matrix --------------------------------------------------
 * Six GPIOs scan the user keypad. On LQFP48 we keep rows on PB3..PB5 and
 * move the columns onto PA7 / PA15 / PB8 so we don't depend on PB9 / PB11
 * being bonded out in every package variant. */

#define PAPR_PIN_KEYPAD_ROW0_PORT    GPIOB
#define PAPR_PIN_KEYPAD_ROW0_PIN     GPIO_PIN_3
#define PAPR_PIN_KEYPAD_ROW1_PORT    GPIOB
#define PAPR_PIN_KEYPAD_ROW1_PIN     GPIO_PIN_4
#define PAPR_PIN_KEYPAD_ROW2_PORT    GPIOB
#define PAPR_PIN_KEYPAD_ROW2_PIN     GPIO_PIN_5

#define PAPR_PIN_KEYPAD_COL0_PORT    GPIOA
#define PAPR_PIN_KEYPAD_COL0_PIN     GPIO_PIN_7
#define PAPR_PIN_KEYPAD_COL1_PORT    GPIOA
#define PAPR_PIN_KEYPAD_COL1_PIN     GPIO_PIN_15
#define PAPR_PIN_KEYPAD_COL2_PORT    GPIOB
#define PAPR_PIN_KEYPAD_COL2_PIN     GPIO_PIN_8

/* ---- Production test (DFT) ---------------------------------------------- */

#define PAPR_PIN_TESTMODE_PORT       GPIOC
#define PAPR_PIN_TESTMODE_PIN        GPIO_PIN_12

#endif /* PAPR_PINMAP_GD32E503CE_H */
