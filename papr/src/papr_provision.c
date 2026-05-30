#include "papr_provision.h"
#include "papr_hal.h"

#include <string.h>

static uint32_t crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0U; i < len; ++i)
    {
        crc ^= data[i];
        for (uint8_t b = 0U; b < 8U; ++b)
        {
            crc = (crc & 1U) ? ((crc >> 1) ^ 0xEDB88320U) : (crc >> 1);
        }
    }
    return crc ^ 0xFFFFFFFFU;
}

static uint32_t record_crc(const papr_provision_t *p)
{
    /* CRC over every byte except the trailing crc32 field itself. */
    return crc32((const uint8_t *)p, sizeof(*p) - sizeof(p->crc32));
}

bool papr_provision_valid(const papr_provision_t *p)
{
    if (p == NULL) { return false; }
    if (p->magic != PAPR_PROV_MAGIC) { return false; }
    if (p->struct_ver != PAPR_PROV_STRUCT_VER) { return false; }
    return p->crc32 == record_crc(p);
}

papr_status_t papr_provision_load(papr_provision_t *out)
{
    if (out == NULL) { return PAPR_ERR_PARAM; }
    papr_status_t s = papr_hal_prov_read((uint8_t *)out, sizeof(*out));
    if (s != PAPR_OK) { return s; }
    return papr_provision_valid(out) ? PAPR_OK : PAPR_ERR_NOT_READY;
}

papr_status_t papr_provision_save(papr_provision_t *p)
{
    if (p == NULL) { return PAPR_ERR_PARAM; }
    p->magic      = PAPR_PROV_MAGIC;
    p->struct_ver = PAPR_PROV_STRUCT_VER;
    p->crc32      = record_crc(p);
    return papr_hal_prov_write((const uint8_t *)p, sizeof(*p));
}

uint16_t papr_provision_apply_flow(const papr_provision_t *p, uint16_t raw_lpm)
{
    if (!papr_provision_valid(p)) { return raw_lpm; }
    int32_t gain = (p->flow_gain_q8 != 0) ? p->flow_gain_q8 : 256;
    int32_t v    = (((int32_t)raw_lpm * gain) >> 8) + p->flow_offset_lpm;
    if (v < 0) { v = 0; }
    if (v > 0xFFFF) { v = 0xFFFF; }
    return (uint16_t)v;
}

uint16_t papr_provision_apply_ibat(const papr_provision_t *p, uint16_t raw_ma)
{
    if (!papr_provision_valid(p)) { return raw_ma; }
    int32_t v = (int32_t)raw_ma - p->ibat_offset_ma;
    if (v < 0) { v = 0; }
    if (v > 0xFFFF) { v = 0xFFFF; }
    return (uint16_t)v;
}

void papr_provision_serial(const papr_provision_t *p, char *out)
{
    if (out == NULL) { return; }

    if (papr_provision_valid(p))
    {
        memcpy(out, p->serial, PAPR_PROV_SERIAL_LEN);
        out[PAPR_PROV_SERIAL_LEN] = '\0';
        return;
    }

    /* Fallback: derive a printable ID from the MCU unique die ID so an
     * unprovisioned board is still trackable at the station. */
    uint8_t uid[12];
    papr_hal_unique_id(uid);
    static const char hex[] = "0123456789ABCDEF";
    out[0] = 'U';
    out[1] = 'N';
    out[2] = '-';
    for (uint8_t i = 0U; i < 6U; ++i)
    {
        out[3U + i * 2U]      = hex[(uid[i] >> 4) & 0xFU];
        out[3U + i * 2U + 1U] = hex[uid[i] & 0xFU];
    }
    out[15] = '\0';
}
