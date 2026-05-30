#include "papr_gdy1124.h"
#include "papr_hal.h"

#include <string.h>

static papr_status_t write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = { reg, value };
    return papr_hal_i2c_write(PAPR_GDY1124_I2C_ADDR, buf, sizeof(buf));
}

static papr_status_t read_regs(uint8_t reg, uint8_t *out, size_t len)
{
    papr_status_t s = papr_hal_i2c_write(PAPR_GDY1124_I2C_ADDR, &reg, 1U);
    if (s != PAPR_OK) { return s; }
    return papr_hal_i2c_read(PAPR_GDY1124_I2C_ADDR, out, len);
}

static papr_status_t load_calibration(papr_gdy1124_t *s)
{
    uint8_t buf[PAPR_GDY1124_CALIB_LEN];
    papr_status_t st = read_regs(PAPR_GDY1124_REG_CALIB_BASE,
                                 buf, sizeof(buf));
    if (st != PAPR_OK) { return st; }

    s->T1 = (uint16_t)((uint16_t)buf[1]  << 8 | buf[0]);
    s->T2 = (int16_t) ((uint16_t)buf[3]  << 8 | buf[2]);
    s->T3 = (int16_t) ((uint16_t)buf[5]  << 8 | buf[4]);
    s->P1 = (uint16_t)((uint16_t)buf[7]  << 8 | buf[6]);
    s->P2 = (int16_t) ((uint16_t)buf[9]  << 8 | buf[8]);
    s->P3 = (int16_t) ((uint16_t)buf[11] << 8 | buf[10]);
    s->P4 = (int16_t) ((uint16_t)buf[13] << 8 | buf[12]);
    s->P5 = (int16_t) ((uint16_t)buf[15] << 8 | buf[14]);
    s->P6 = (int16_t) ((uint16_t)buf[17] << 8 | buf[16]);
    s->P7 = (int16_t) ((uint16_t)buf[19] << 8 | buf[18]);
    s->P8 = (int16_t) ((uint16_t)buf[21] << 8 | buf[20]);
    s->P9 = (int16_t) ((uint16_t)buf[23] << 8 | buf[22]);
    return PAPR_OK;
}

papr_status_t papr_gdy1124_soft_reset(papr_gdy1124_t *s)
{
    if (s == NULL) { return PAPR_ERR_PARAM; }
    s->valid = false;
    return write_reg(PAPR_GDY1124_REG_RESET, PAPR_GDY1124_RESET_MAGIC);
}

papr_status_t papr_gdy1124_init(papr_gdy1124_t *s)
{
    if (s == NULL) { return PAPR_ERR_PARAM; }
    memset(s, 0, sizeof(*s));

    uint8_t id = 0U;
    papr_status_t st = read_regs(PAPR_GDY1124_REG_CHIP_ID, &id, 1U);
    if (st != PAPR_OK) { return st; }
    if (id != PAPR_GDY1124_CHIP_ID) { return PAPR_ERR_HW; }
    s->present = true;

    st = load_calibration(s);
    if (st != PAPR_OK) { return st; }

    /* Standby 250 ms, IIR filter ×4, no SPI 3-wire. */
    st = write_reg(PAPR_GDY1124_REG_CONFIG, (uint8_t)((0x3U << 5) | (0x2U << 2)));
    if (st != PAPR_OK) { return st; }

    return write_reg(PAPR_GDY1124_REG_CTRL_MEAS, PAPR_GDY1124_CTRL_NORMAL);
}

/* Bosch-class compensation algorithm in 32-bit fixed-point. The structure is
 * the same across this family of barometric sensors; coefficient names map
 * 1:1 to the calibration block read above. */
static int32_t compensate_temperature(papr_gdy1124_t *s, int32_t adc_T)
{
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)s->T1 << 1))) * (int32_t)s->T2) >> 11;
    int32_t var2 = (((((adc_T >> 4) - (int32_t)s->T1) *
                      ((adc_T >> 4) - (int32_t)s->T1)) >> 12) * (int32_t)s->T3) >> 14;
    s->t_fine = var1 + var2;
    return (s->t_fine * 5 + 128) >> 8;        /* °C * 100 */
}

static uint32_t compensate_pressure(papr_gdy1124_t *s, int32_t adc_P)
{
    int64_t var1 = (int64_t)s->t_fine - 128000;
    int64_t var2 = var1 * var1 * (int64_t)s->P6;
    var2  += (var1 * (int64_t)s->P5) << 17;
    var2  += ((int64_t)s->P4) << 35;
    var1   = ((var1 * var1 * (int64_t)s->P3) >> 8) +
             ((var1 * (int64_t)s->P2) << 12);
    var1   = (((((int64_t)1) << 47) + var1) * (int64_t)s->P1) >> 33;
    if (var1 == 0) { return 0U; }

    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = ((int64_t)s->P9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = ((int64_t)s->P8 * p) >> 19;
    p = ((p + var1 + var2) >> 8) + ((int64_t)s->P7 << 4);
    /* p is in Q24.8 Pa; shift to integer Pa. */
    return (uint32_t)(p >> 8);
}

papr_status_t papr_gdy1124_read(papr_gdy1124_t *s)
{
    if (s == NULL) { return PAPR_ERR_PARAM; }
    if (!s->present) { return PAPR_ERR_NOT_READY; }

    uint8_t raw[6];
    papr_status_t st = read_regs(PAPR_GDY1124_REG_PRESS_MSB,
                                 raw, sizeof(raw));
    if (st != PAPR_OK) { s->valid = false; return st; }

    int32_t adc_P = ((int32_t)raw[0] << 12) |
                    ((int32_t)raw[1] << 4)  |
                    ((int32_t)raw[2] >> 4);
    int32_t adc_T = ((int32_t)raw[3] << 12) |
                    ((int32_t)raw[4] << 4)  |
                    ((int32_t)raw[5] >> 4);

    int32_t  t_c100   = compensate_temperature(s, adc_T);
    uint32_t p_pa     = compensate_pressure(s, adc_P);

    /* °C×100 → 0.1 °C with rounding. */
    int32_t t_c10 = (t_c100 + ((t_c100 >= 0) ? 5 : -5)) / 10;
    if (t_c10 >  32767) { t_c10 =  32767; }
    if (t_c10 < -32768) { t_c10 = -32768; }

    s->pressure_pa     = p_pa;
    s->temperature_c10 = (int16_t)t_c10;
    s->valid           = true;
    return PAPR_OK;
}

uint32_t papr_gdy1124_pressure_pa(const papr_gdy1124_t *s)
{
    return (s == NULL) ? 0U : s->pressure_pa;
}

int16_t papr_gdy1124_temperature_c10(const papr_gdy1124_t *s)
{
    return (s == NULL) ? 0 : s->temperature_c10;
}

bool papr_gdy1124_valid(const papr_gdy1124_t *s)
{
    return (s != NULL) && s->valid;
}
