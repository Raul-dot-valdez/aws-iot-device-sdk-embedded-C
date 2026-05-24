#include "papr_ota.h"
#include "papr_hal.h"

#include <string.h>

/* Reflected CRC-32 (poly 0xEDB88320), the zlib / Ethernet variant, so the app
 * can precompute the image CRC with any stock library. crc32_step folds bytes
 * into a running register without applying the init/final inversions, letting
 * the caller stream an image in arbitrary block sizes. */
static uint32_t crc32_step(uint32_t running, const uint8_t *data, size_t len)
{
    for (size_t i = 0U; i < len; ++i)
    {
        running ^= data[i];
        for (uint8_t b = 0U; b < 8U; ++b)
        {
            running = (running & 1U) ? ((running >> 1) ^ 0xEDB88320U)
                                     : (running >> 1);
        }
    }
    return running;
}

papr_status_t papr_ota_init(papr_ota_t *o)
{
    if (o == NULL) { return PAPR_ERR_PARAM; }
    memset(o, 0, sizeof(*o));
    o->state = PAPR_OTA_IDLE;
    o->error = PAPR_OTA_ERR_NONE;
    return PAPR_OK;
}

static papr_ota_error_t fail(papr_ota_t *o, papr_ota_error_t e)
{
    o->state = PAPR_OTA_ERROR;
    o->error = e;
    return e;
}

papr_ota_error_t papr_ota_begin(papr_ota_t *o,
                                uint32_t image_size,
                                uint32_t image_crc32,
                                papr_fw_version_t incoming,
                                bool device_idle)
{
    if (o == NULL) { return PAPR_OTA_ERR_STATE; }
    if (!device_idle) { return fail(o, PAPR_OTA_ERR_BUSY); }
    if (image_size == 0U || image_size > papr_hal_ota_slot_size())
    {
        return fail(o, PAPR_OTA_ERR_SIZE);
    }
#if PAPR_OTA_REJECT_DOWNGRADE
    if (papr_version_cmp(incoming, papr_version_current()) < 0)
    {
        return fail(o, PAPR_OTA_ERR_VERSION);
    }
#endif

    if (papr_hal_ota_erase() != PAPR_OK)
    {
        return fail(o, PAPR_OTA_ERR_FLASH);
    }

    o->state          = PAPR_OTA_RECEIVING;
    o->error          = PAPR_OTA_ERR_NONE;
    o->image_size     = image_size;
    o->received       = 0U;
    o->next_offset    = 0U;
    o->expected_crc32 = image_crc32;
    o->incoming       = incoming;
    return PAPR_OTA_ERR_NONE;
}

papr_ota_error_t papr_ota_write(papr_ota_t *o,
                                uint32_t offset,
                                const uint8_t *data,
                                uint16_t len)
{
    if (o == NULL || data == NULL) { return PAPR_OTA_ERR_STATE; }
    if (o->state != PAPR_OTA_RECEIVING) { return fail(o, PAPR_OTA_ERR_STATE); }
    if (len == 0U) { return PAPR_OTA_ERR_NONE; }

    if (offset != o->next_offset) { return fail(o, PAPR_OTA_ERR_SEQUENCE); }
    if ((uint32_t)offset + len > o->image_size)
    {
        return fail(o, PAPR_OTA_ERR_SEQUENCE);
    }

    /* Word alignment is required by the flash controller. The final chunk may
     * be short; the HAL pads the trailing partial word with 0xFF. */
    bool last = ((uint32_t)offset + len == o->image_size);
    if ((offset % PAPR_OTA_WRITE_ALIGN) != 0U) { return fail(o, PAPR_OTA_ERR_ALIGN); }
    if (!last && (len % PAPR_OTA_WRITE_ALIGN) != 0U)
    {
        return fail(o, PAPR_OTA_ERR_ALIGN);
    }

    if (papr_hal_ota_write(offset, data, len) != PAPR_OK)
    {
        return fail(o, PAPR_OTA_ERR_FLASH);
    }

    o->next_offset += len;
    o->received    += len;
    return PAPR_OTA_ERR_NONE;
}

papr_ota_error_t papr_ota_finish(papr_ota_t *o)
{
    if (o == NULL) { return PAPR_OTA_ERR_STATE; }
    if (o->state != PAPR_OTA_RECEIVING) { return fail(o, PAPR_OTA_ERR_STATE); }
    if (o->received != o->image_size)   { return fail(o, PAPR_OTA_ERR_SEQUENCE); }

    /* Recompute the CRC by streaming the staged image back out of flash. */
    uint8_t  buf[64];
    uint32_t running   = 0xFFFFFFFFU;
    uint32_t remaining = o->image_size;
    uint32_t off       = 0U;
    while (remaining > 0U)
    {
        uint32_t n = (remaining < sizeof(buf)) ? remaining : (uint32_t)sizeof(buf);
        if (papr_hal_ota_read(off, buf, n) != PAPR_OK)
        {
            return fail(o, PAPR_OTA_ERR_FLASH);
        }
        running    = crc32_step(running, buf, n);
        off       += n;
        remaining -= n;
        papr_hal_wdt_kick();   /* verification can take a while on big images */
    }
    uint32_t crc = running ^ 0xFFFFFFFFU;

    if (crc != o->expected_crc32) { return fail(o, PAPR_OTA_ERR_CRC); }

    o->state = PAPR_OTA_READY;
    o->error = PAPR_OTA_ERR_NONE;
    return PAPR_OTA_ERR_NONE;
}

papr_ota_error_t papr_ota_apply(papr_ota_t *o)
{
    if (o == NULL) { return PAPR_OTA_ERR_STATE; }
    if (o->state != PAPR_OTA_READY) { return PAPR_OTA_ERR_STATE; }

    if (papr_hal_ota_commit(o->image_size, o->expected_crc32) != PAPR_OK)
    {
        return fail(o, PAPR_OTA_ERR_FLASH);
    }
    /* Caller flushes the acknowledgement, then triggers the reset. */
    return PAPR_OTA_ERR_NONE;
}

void papr_ota_abort(papr_ota_t *o)
{
    if (o == NULL) { return; }
    o->state       = PAPR_OTA_IDLE;
    o->error       = PAPR_OTA_ERR_NONE;
    o->image_size  = 0U;
    o->received    = 0U;
    o->next_offset = 0U;
}

uint8_t papr_ota_progress_percent(const papr_ota_t *o)
{
    if (o == NULL || o->image_size == 0U) { return 0U; }
    if (o->state == PAPR_OTA_READY) { return 100U; }
    uint32_t pct = (uint32_t)(((uint64_t)o->received * 100U) / o->image_size);
    return (pct > 100U) ? 100U : (uint8_t)pct;
}

bool papr_ota_in_progress(const papr_ota_t *o)
{
    return (o != NULL) && (o->state == PAPR_OTA_RECEIVING);
}
