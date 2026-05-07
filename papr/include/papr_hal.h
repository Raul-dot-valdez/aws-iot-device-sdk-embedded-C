#ifndef PAPR_HAL_H
#define PAPR_HAL_H

#include "papr_types.h"

/* Hardware abstraction layer. Implement these symbols once per target MCU
 * (STM32, NXP Kinetis, NRF52, RP2040, ESP32 ...). The controller core is
 * portable and never touches a peripheral register directly.
 *
 * Revision 2: the blower is driven by an ST L6235 three-phase DMOS driver.
 * The MCU no longer generates the bridge PWM itself; instead it controls the
 * L6235 through the following signals:
 *
 *   VREF  — analog (DAC or filtered PWM): peak phase current setpoint
 *   EN    — GPIO out, active high: enables the output bridge
 *   FWD   — GPIO out: rotation direction (forward = blower-out)
 *   BRAKE — GPIO out, active LOW: shorts low-side FETs when asserted
 *   DIAG  — GPIO in, open-drain (low = fault, OCD or thermal shutdown)
 *   TACHO — GPIO in, captured by a timer (pulse train proportional to RPM)
 */

papr_status_t papr_hal_init(void);

/* Monotonic millisecond tick. Must be wrap-safe over 32 bits. */
uint32_t papr_hal_now_ms(void);

/* L6235 actuation. */
papr_status_t papr_hal_l6235_set_vref(uint16_t code);   /* 0..PAPR_L6235_VREF_DAC_MAX */
papr_status_t papr_hal_l6235_set_enable(bool enable);
papr_status_t papr_hal_l6235_set_forward(bool forward);
papr_status_t papr_hal_l6235_set_brake(bool brake_engaged);

/* L6235 status. diag_active() returns true when DIAG is asserted (= LOW on
 * the open-drain pin) and the condition has persisted past the debounce
 * window. tacho_rpm() returns the motor speed measured from the TACHO
 * pulse train, normalised to PAPR_L6235_TACHO_PPR. */
bool          papr_hal_l6235_diag_active(void);
papr_status_t papr_hal_l6235_read_tacho_rpm(uint16_t *out);

/* Sensor reads. Return PAPR_ERR_HW on bus failure.
 * The differential-pressure sensor (Sensirion SDP810) lives behind a portable
 * driver in the core and is *not* a HAL function — it uses the I²C primitives
 * below. */
papr_status_t papr_hal_read_flow_lpm(uint16_t *out);
papr_status_t papr_hal_read_battery_mv(uint16_t *out);
papr_status_t papr_hal_read_battery_ma(uint16_t *out);
papr_status_t papr_hal_read_temperature_c10(int16_t *out);

/* Raw I²C transport. The slave address is the 7-bit form (LSB = 0).
 * Implementations must perform a complete START / addr / payload / STOP
 * transaction. A repeated-start read-after-write is exposed as a separate
 * helper so SDP810 reads (which use a plain stop-then-start) and other
 * sensors that require Sr both fit naturally. */
papr_status_t papr_hal_i2c_write(uint8_t addr7,
                                 const uint8_t *data, size_t len);
papr_status_t papr_hal_i2c_read(uint8_t addr7,
                                uint8_t *data, size_t len);

/* User interface: button, LEDs, buzzer. */
bool papr_hal_button_power_pressed(void);
bool papr_hal_button_level_pressed(void);
papr_status_t papr_hal_led_set(uint8_t led_id, bool on);
papr_status_t papr_hal_buzzer_set(bool on, uint16_t freq_hz);

/* Independent watchdog. */
void papr_hal_wdt_kick(void);

#endif /* PAPR_HAL_H */
