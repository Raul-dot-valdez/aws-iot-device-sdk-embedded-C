#ifndef PAPR_SDP810_H
#define PAPR_SDP810_H

#include "papr_types.h"

/* Driver for the Sensirion SDP810-500Pa differential pressure sensor.
 *
 * Pin-out (4-pin tube-connection variant):
 *
 *     1  VDD  (3.0–3.6 V)
 *     2  SDA  (I²C data, open-drain, 4.7 kΩ pull-up to VDD)
 *     3  GND
 *     4  SCL  (I²C clock, open-drain, 4.7 kΩ pull-up to VDD)
 *
 * Default I²C address: 0x25 (7-bit). Range ±500 Pa, scale factor 60 LSB/Pa,
 * on-chip temperature compensation, internal 2nd-order IIR filter.
 *
 * Protocol summary used here (datasheet rev. 1.1, ch. 5):
 *   - "Start continuous measurement, differential pressure, with averaging":
 *     write 0x36 0x1E. The chip then refreshes its result registers
 *     internally; ≈ 8 ms after the command the first 9-byte read is valid.
 *   - Read 9 bytes:
 *           [0..1]  signed dP (big-endian)
 *           [2]     CRC-8 over bytes 0..1
 *           [3..4]  signed temperature (big-endian, 1/200 °C)
 *           [5]     CRC-8 over bytes 3..4
 *           [6..7]  scale factor (unsigned big-endian, LSB/Pa)
 *           [8]     CRC-8 over bytes 6..7
 *   - "Stop continuous measurement": write 0x3F 0xF9. */

typedef struct
{
    int16_t  pressure_pa;
    int16_t  temperature_c10;
    uint16_t scale_factor;
    uint32_t last_start_ms;
    bool     started;
    bool     valid;
} papr_sdp810_t;

papr_status_t papr_sdp810_init(papr_sdp810_t *s);
papr_status_t papr_sdp810_start(papr_sdp810_t *s);
papr_status_t papr_sdp810_stop(papr_sdp810_t *s);
papr_status_t papr_sdp810_soft_reset(papr_sdp810_t *s);

/* Reads one fresh 9-byte frame, validates the three CRCs, and updates the
 * cached pressure / temperature / scale fields. Returns PAPR_ERR_HW on
 * transport failure and PAPR_ERR_PARAM on CRC mismatch. */
papr_status_t papr_sdp810_read(papr_sdp810_t *s);

int16_t  papr_sdp810_pressure_pa(const papr_sdp810_t *s);
int16_t  papr_sdp810_temperature_c10(const papr_sdp810_t *s);
uint16_t papr_sdp810_scale_factor(const papr_sdp810_t *s);
bool     papr_sdp810_valid(const papr_sdp810_t *s);

/* Public CRC helper. Sensirion CRC-8: poly 0x31, init 0xFF, no final XOR.
 * Exposed so HAL test stubs can fabricate frames without duplicating code. */
uint8_t papr_sdp810_crc8(const uint8_t *data, size_t len);

#endif /* PAPR_SDP810_H */
