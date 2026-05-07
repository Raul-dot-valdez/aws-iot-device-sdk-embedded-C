#include "papr_sdp810.h"
#include "papr_config.h"
#include "papr_hal.h"

#define CMD_START_CONT_DP_AVG_HI    0x36U
#define CMD_START_CONT_DP_AVG_LO    0x1EU
#define CMD_STOP_HI                 0x3FU
#define CMD_STOP_LO                 0xF9U
#define CMD_SOFT_RESET_HI           0x00U
#define CMD_SOFT_RESET_LO           0x06U  /* general-call address 0x00 */

#define FRAME_LEN                   9U

uint8_t papr_sdp810_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFFU;
    for (size_t i = 0U; i < len; ++i)
    {
        crc ^= data[i];
        for (uint8_t b = 0U; b < 8U; ++b)
        {
            crc = (crc & 0x80U) ? (uint8_t)((crc << 1) ^ 0x31U)
                                : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static papr_status_t send_cmd(uint8_t addr, uint8_t hi, uint8_t lo)
{
    uint8_t buf[2] = { hi, lo };
    return papr_hal_i2c_write(addr, buf, sizeof(buf));
}

papr_status_t papr_sdp810_init(papr_sdp810_t *s)
{
    if (s == NULL) { return PAPR_ERR_PARAM; }
    s->pressure_pa     = 0;
    s->temperature_c10 = 0;
    s->scale_factor    = (uint16_t)PAPR_SDP810_SCALE;
    s->last_start_ms   = 0U;
    s->started         = false;
    s->valid           = false;
    return papr_sdp810_start(s);
}

papr_status_t papr_sdp810_start(papr_sdp810_t *s)
{
    if (s == NULL) { return PAPR_ERR_PARAM; }
    papr_status_t st = send_cmd(PAPR_SDP810_I2C_ADDR,
                                CMD_START_CONT_DP_AVG_HI,
                                CMD_START_CONT_DP_AVG_LO);
    if (st != PAPR_OK)
    {
        s->started = false;
        return st;
    }
    s->started       = true;
    s->last_start_ms = papr_hal_now_ms();
    return PAPR_OK;
}

papr_status_t papr_sdp810_stop(papr_sdp810_t *s)
{
    if (s == NULL) { return PAPR_ERR_PARAM; }
    s->started = false;
    return send_cmd(PAPR_SDP810_I2C_ADDR, CMD_STOP_HI, CMD_STOP_LO);
}

papr_status_t papr_sdp810_soft_reset(papr_sdp810_t *s)
{
    if (s == NULL) { return PAPR_ERR_PARAM; }
    s->started = false;
    s->valid   = false;
    return send_cmd(0x00U, CMD_SOFT_RESET_HI, CMD_SOFT_RESET_LO);
}

papr_status_t papr_sdp810_read(papr_sdp810_t *s)
{
    if (s == NULL) { return PAPR_ERR_PARAM; }

    /* Sensor must be in continuous mode and the start-up window must have
     * elapsed before the first frame is valid. */
    if (!s->started)
    {
        papr_status_t st = papr_sdp810_start(s);
        if (st != PAPR_OK) { return st; }
    }
    uint32_t now = papr_hal_now_ms();
    if ((uint32_t)(now - s->last_start_ms) < PAPR_SDP810_STARTUP_DELAY_MS)
    {
        return PAPR_ERR_NOT_READY;
    }

    uint8_t buf[FRAME_LEN];
    papr_status_t st = papr_hal_i2c_read(PAPR_SDP810_I2C_ADDR, buf, FRAME_LEN);
    if (st != PAPR_OK)
    {
        s->valid   = false;
        s->started = false;   /* force restart on next call */
        return st;
    }

    if (papr_sdp810_crc8(&buf[0], 2U) != buf[2] ||
        papr_sdp810_crc8(&buf[3], 2U) != buf[5] ||
        papr_sdp810_crc8(&buf[6], 2U) != buf[8])
    {
        s->valid = false;
        return PAPR_ERR_PARAM;
    }

    int16_t  raw_dp    = (int16_t)((uint16_t)buf[0] << 8 | buf[1]);
    int16_t  raw_temp  = (int16_t)((uint16_t)buf[3] << 8 | buf[4]);
    uint16_t scale     = (uint16_t)((uint16_t)buf[6] << 8 | buf[7]);
    if (scale == 0U) { scale = (uint16_t)PAPR_SDP810_SCALE; }

    /* Pressure in Pa = raw / scale, rounded toward zero. */
    int32_t pressure = (int32_t)raw_dp / (int32_t)scale;
    if (pressure >  32767) { pressure =  32767; }
    if (pressure < -32768) { pressure = -32768; }

    /* Temperature in 0.1 °C = raw / 200 * 10 = raw / 20. */
    int32_t temp_c10 = (int32_t)raw_temp / 20;
    if (temp_c10 >  32767) { temp_c10 =  32767; }
    if (temp_c10 < -32768) { temp_c10 = -32768; }

    s->pressure_pa     = (int16_t)pressure;
    s->temperature_c10 = (int16_t)temp_c10;
    s->scale_factor    = scale;
    s->valid           = true;

    /* Periodic re-arming guards against a spurious bus glitch having
     * silently knocked the chip out of continuous mode. */
    if ((uint32_t)(now - s->last_start_ms) > PAPR_SDP810_RESTART_PERIOD_MS)
    {
        (void)papr_sdp810_start(s);
    }
    return PAPR_OK;
}

int16_t papr_sdp810_pressure_pa(const papr_sdp810_t *s)
{
    return (s == NULL) ? 0 : s->pressure_pa;
}

int16_t papr_sdp810_temperature_c10(const papr_sdp810_t *s)
{
    return (s == NULL) ? 0 : s->temperature_c10;
}

uint16_t papr_sdp810_scale_factor(const papr_sdp810_t *s)
{
    return (s == NULL) ? 0U : s->scale_factor;
}

bool papr_sdp810_valid(const papr_sdp810_t *s)
{
    return (s != NULL) && s->valid;
}
