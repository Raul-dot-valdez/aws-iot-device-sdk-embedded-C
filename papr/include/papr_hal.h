#ifndef PAPR_HAL_H
#define PAPR_HAL_H

#include "papr_types.h"

/* Hardware abstraction layer. Implement these symbols once per target MCU
 * (STM32, NXP Kinetis, NRF52, RP2040, ESP32 ...). The controller core is
 * portable and never touches a peripheral register directly. */

papr_status_t papr_hal_init(void);

/* Monotonic millisecond tick. Must be wrap-safe over 32 bits. */
uint32_t papr_hal_now_ms(void);

/* PWM driver for the brushless blower. duty is 0..PAPR_PWM_MAX. */
papr_status_t papr_hal_blower_set_duty(uint16_t duty);
papr_status_t papr_hal_blower_enable(bool enable);

/* Sensor reads. Return PAPR_ERR_HW on bus failure. */
papr_status_t papr_hal_read_flow_lpm(uint16_t *out);
papr_status_t papr_hal_read_pressure_pa(uint16_t *out);
papr_status_t papr_hal_read_battery_mv(uint16_t *out);
papr_status_t papr_hal_read_battery_ma(uint16_t *out);
papr_status_t papr_hal_read_motor_rpm(uint16_t *out);
papr_status_t papr_hal_read_temperature_c10(int16_t *out);

/* User interface: button, LEDs, buzzer. */
bool papr_hal_button_power_pressed(void);
bool papr_hal_button_level_pressed(void);
papr_status_t papr_hal_led_set(uint8_t led_id, bool on);
papr_status_t papr_hal_buzzer_set(bool on, uint16_t freq_hz);

/* Independent watchdog. */
void papr_hal_wdt_kick(void);

#endif /* PAPR_HAL_H */
