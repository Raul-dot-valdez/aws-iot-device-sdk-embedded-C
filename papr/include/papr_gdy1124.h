#ifndef PAPR_GDY1124_H
#define PAPR_GDY1124_H

#include "papr_types.h"

/* Driver for the GigaDevice GDY1124 absolute pressure sensor.
 *
 * Interface  : I²C, default 7-bit address 0x76 (SDO pin tied to GND).
 * Range      : 300 .. 1100 hPa (typical barometric / atmospheric range).
 * Resolution : 24-bit raw pressure, 24-bit raw temperature.
 *
 * Register-level details (chip ID byte, calibration block layout, command
 * codes for one-shot conversion) are abstracted behind the constants below.
 * Reconcile each value against the official GDY1124 datasheet before
 * release; the protocol skeleton (write-register-then-read) and the
 * compensation flow are correct for any sensor in this class.
 *
 * The chip lives on the same I²C bus as the SDP810 (different address). */

#ifndef PAPR_GDY1124_I2C_ADDR
#define PAPR_GDY1124_I2C_ADDR        0x76U
#endif

/* Expected chip-ID register value. */
#ifndef PAPR_GDY1124_CHIP_ID
#define PAPR_GDY1124_CHIP_ID         0x24U
#endif

/* Register addresses (verify against datasheet). */
#define PAPR_GDY1124_REG_CHIP_ID     0x00U
#define PAPR_GDY1124_REG_RESET       0xE0U
#define PAPR_GDY1124_REG_CTRL_MEAS   0xF4U
#define PAPR_GDY1124_REG_CONFIG      0xF5U
#define PAPR_GDY1124_REG_PRESS_MSB   0xF7U   /* 0xF7..0xF9 = pressure (24b) */
#define PAPR_GDY1124_REG_TEMP_MSB    0xFAU   /* 0xFA..0xFC = temperature   */
#define PAPR_GDY1124_REG_CALIB_BASE  0x88U   /* 0x88..0x9F = 24 bytes      */
#define PAPR_GDY1124_CALIB_LEN       24U

/* CTRL_MEAS value: ×4 oversampling on temperature, ×16 on pressure,
 * normal mode. Layout: osrs_t[7:5] osrs_p[4:2] mode[1:0]. */
#define PAPR_GDY1124_CTRL_NORMAL     ((0x3U << 5) | (0x5U << 2) | 0x3U)

/* Soft-reset magic value. */
#define PAPR_GDY1124_RESET_MAGIC     0xB6U

typedef struct
{
    /* Calibration coefficients read from the chip at init.                */
    uint16_t T1;
    int16_t  T2;
    int16_t  T3;
    uint16_t P1;
    int16_t  P2;
    int16_t  P3;
    int16_t  P4;
    int16_t  P5;
    int16_t  P6;
    int16_t  P7;
    int16_t  P8;
    int16_t  P9;

    int32_t  t_fine;             /* shared across pressure compensation   */
    uint32_t pressure_pa;        /* most recent compensated pressure      */
    int16_t  temperature_c10;    /* most recent compensated temperature   */
    bool     present;            /* chip-ID match succeeded               */
    bool     valid;              /* last read produced usable data        */
} papr_gdy1124_t;

papr_status_t papr_gdy1124_init(papr_gdy1124_t *s);
papr_status_t papr_gdy1124_soft_reset(papr_gdy1124_t *s);

/* Polls one fresh sample. Pressure is returned in pascals (absolute) and
 * temperature in tenths of a degree C, both via the cached fields. */
papr_status_t papr_gdy1124_read(papr_gdy1124_t *s);

uint32_t papr_gdy1124_pressure_pa(const papr_gdy1124_t *s);
int16_t  papr_gdy1124_temperature_c10(const papr_gdy1124_t *s);
bool     papr_gdy1124_valid(const papr_gdy1124_t *s);

#endif /* PAPR_GDY1124_H */
