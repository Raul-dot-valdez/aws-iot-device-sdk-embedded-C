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

/* UART transport for the GD32VW553-UNIFI-EMH7 BLE module. The HAL owns a
 * small ring buffer fed by the RX interrupt; the portable papr_ble module
 * polls it via papr_hal_uart_read_byte(). Writes are synchronous-blocking
 * with a built-in timeout suitable for telemetry packets up to ~256 bytes. */
papr_status_t papr_hal_uart_write(const uint8_t *data, size_t len);

/* Returns true and stores the next byte in *out if one is available.
 * Returns false if the RX buffer is empty. Non-blocking. */
bool papr_hal_uart_read_byte(uint8_t *out);

/* BLE module control lines (active-low reset, host-wake input). */
papr_status_t papr_hal_ble_set_reset(bool asserted);
bool          papr_hal_ble_host_wake(void);

/* User interface: legacy buttons (kept for backward compat — wired in the
 * GD32E517RE port but no longer driving the controller), 3x3 switch matrix
 * (primary input path in rev 6), LEDs, buzzer. */
bool papr_hal_button_power_pressed(void);
bool papr_hal_button_level_pressed(void);

/* Drives one row of the keypad to its active level. Active level is HAL-
 * defined (typically LOW with internal pull-ups on the column inputs). */
papr_status_t papr_hal_keypad_drive_row(uint8_t row, bool active);

/* Reads one column input. Returns true when the line is at its active
 * level, i.e. when a key in the currently driven row is depressed. */
bool papr_hal_keypad_read_col(uint8_t col);

papr_status_t papr_hal_led_set(uint8_t led_id, bool on);
papr_status_t papr_hal_buzzer_set(bool on, uint16_t freq_hz);

/* Independent watchdog. */
void papr_hal_wdt_kick(void);

/* OTA / firmware update flash access. The "staging slot" is the inactive
 * application bank; all offsets are relative to its base. Implementations
 * must keep the watchdog fed during long erases (erase can take tens of ms).
 *
 *   slot_size()  usable bytes in the staging slot
 *   erase()      erase the whole staging slot, ready for programming
 *   write()      program len bytes at offset (offset+len within the slot;
 *                offset and len are PAPR_OTA_WRITE_ALIGN-aligned)
 *   read()       read back len bytes from offset (for CRC verification)
 *   commit()     persist the boot metadata (size + CRC + "pending" flag) so
 *                the bootloader runs the staged image on next reset
 *   reboot()     trigger a system reset (does not return on real hardware)
 */
uint32_t      papr_hal_ota_slot_size(void);
papr_status_t papr_hal_ota_erase(void);
papr_status_t papr_hal_ota_write(uint32_t offset, const uint8_t *data, uint32_t len);
papr_status_t papr_hal_ota_read(uint32_t offset, uint8_t *data, uint32_t len);
papr_status_t papr_hal_ota_commit(uint32_t size, uint32_t crc32);
void          papr_hal_ota_reboot(void);

/* Production test / provisioning (Design-for-Test).
 *
 *   factory_requested()  sampled at boot — true when the end-of-line test
 *                        fixture asserts the TEST_MODE pad. Safe to call
 *                        before papr_hal_init(); it brings up only what it
 *                        needs to read the pad.
 *   unique_id()          MCU die unique ID (12 bytes) — used as a fallback
 *                        serial for an unprovisioned board.
 *   prov_read/write()    access the dedicated provisioning flash page (kept
 *                        separate from the OTA slots so it survives updates).
 */
bool          papr_hal_factory_requested(void);
void          papr_hal_unique_id(uint8_t out[12]);
papr_status_t papr_hal_prov_read(uint8_t *data, uint32_t len);
papr_status_t papr_hal_prov_write(const uint8_t *data, uint32_t len);

#endif /* PAPR_HAL_H */
