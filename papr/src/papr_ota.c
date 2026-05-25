#include "papr_ota.h"
#include "papr_hal.h"

#include <string.h>

papr_status_t papr_ota_init(papr_ota_t *o, papr_secure_t *sec)
{
    if (o == NULL) { return PAPR_ERR_PARAM; }
    memset(o, 0, sizeof(*o));
    o->state = PAPR_OTA_IDLE;
    o->error = PAPR_OTA_ERR_NONE;
    o->sec   = sec;
    return PAPR_OK;
}

static papr_ota_error_t fail(papr_ota_t *o, papr_ota_error_t e)
{
    o->state = PAPR_OTA_ERROR;
    o->error = e;
    return e;
}

papr_ota_error_t papr_ota_begin(papr_ota_t *o,
                                const papr_img_manifest_t *manifest,
                                bool device_idle)
{
    if (o == NULL || manifest == NULL) { return PAPR_OTA_ERR_STATE; }
    if (!device_idle) { return fail(o, PAPR_OTA_ERR_BUSY); }
    if (manifest->magic != PAPR_IMG_MAGIC) { return fail(o, PAPR_OTA_ERR_AUTH); }
    if (manifest->image_size == 0U ||
        manifest->image_size > papr_hal_ota_slot_size())
    {
        return fail(o, PAPR_OTA_ERR_SIZE);
    }
    /* Cheap early anti-rollback reject so we don't bother flashing an image
     * we will refuse anyway (the authoritative check is at finish). */
    if (manifest->sec_version < papr_hal_sec_version_get())
    {
        return fail(o, PAPR_OTA_ERR_ROLLBACK);
    }

    if (papr_hal_ota_erase() != PAPR_OK) { return fail(o, PAPR_OTA_ERR_FLASH); }

    o->state       = PAPR_OTA_RECEIVING;
    o->error       = PAPR_OTA_ERR_NONE;
    o->manifest    = *manifest;
    o->image_size  = manifest->image_size;
    o->received    = 0U;
    o->next_offset = 0U;
    papr_sha256_init(&o->hash);
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
    if ((uint32_t)offset + len > o->image_size) { return fail(o, PAPR_OTA_ERR_SEQUENCE); }

    bool last = ((uint32_t)offset + len == o->image_size);
    if ((offset % PAPR_OTA_WRITE_ALIGN) != 0U) { return fail(o, PAPR_OTA_ERR_ALIGN); }
    if (!last && (len % PAPR_OTA_WRITE_ALIGN) != 0U) { return fail(o, PAPR_OTA_ERR_ALIGN); }

    if (papr_hal_ota_write(offset, data, len) != PAPR_OK)
    {
        return fail(o, PAPR_OTA_ERR_FLASH);
    }
    papr_sha256_update(&o->hash, data, len);

    o->next_offset += len;
    o->received    += len;
    return PAPR_OTA_ERR_NONE;
}

papr_ota_error_t papr_ota_finish(papr_ota_t *o)
{
    if (o == NULL) { return PAPR_OTA_ERR_STATE; }
    if (o->state != PAPR_OTA_RECEIVING) { return fail(o, PAPR_OTA_ERR_STATE); }
    if (o->received != o->image_size)   { return fail(o, PAPR_OTA_ERR_SEQUENCE); }

    uint8_t sha[PAPR_SHA256_LEN];
    papr_sha256_final(&o->hash, sha);

#if PAPR_SEC_REQUIRE_SIGNED_OTA
    papr_sec_result_t r = papr_secure_verify_image(o->sec, &o->manifest, sha,
                                                   papr_hal_sec_version_get());
    switch (r)
    {
        case PAPR_SEC_OK:            break;
        case PAPR_SEC_ERR_HASH:      return fail(o, PAPR_OTA_ERR_HASH);
        case PAPR_SEC_ERR_ROLLBACK:  return fail(o, PAPR_OTA_ERR_ROLLBACK);
        default:                     return fail(o, PAPR_OTA_ERR_AUTH);
    }
#else
    /* Integrity-only mode: still verify the image hashes to the manifest. */
    if (!papr_ct_equal(sha, o->manifest.sha256, PAPR_SHA256_LEN))
    {
        return fail(o, PAPR_OTA_ERR_HASH);
    }
#endif

    o->state = PAPR_OTA_READY;
    o->error = PAPR_OTA_ERR_NONE;
    return PAPR_OTA_ERR_NONE;
}

papr_ota_error_t papr_ota_apply(papr_ota_t *o)
{
    if (o == NULL) { return PAPR_OTA_ERR_STATE; }
    if (o->state != PAPR_OTA_READY) { return PAPR_OTA_ERR_STATE; }

    /* Advance the anti-rollback floor and persist the signed manifest into
     * the slot trailer; the bootloader re-verifies it before booting. */
    if (o->manifest.sec_version > papr_hal_sec_version_get())
    {
        (void)papr_hal_sec_version_set(o->manifest.sec_version);
    }
    if (papr_hal_ota_commit(&o->manifest) != PAPR_OK)
    {
        return fail(o, PAPR_OTA_ERR_FLASH);
    }
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
