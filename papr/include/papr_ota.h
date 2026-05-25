#ifndef PAPR_OTA_H
#define PAPR_OTA_H

#include "papr_config.h"
#include "papr_secure.h"
#include "papr_sha256.h"
#include "papr_types.h"
#include "papr_version.h"
#include "../boot/boot_shared.h"

/* Over-the-air firmware update receiver (secure).
 *
 * The mobile app first sends a signed manifest (OTA_BEGIN): image size,
 * security version, firmware version, the SHA-256 of the image, and the
 * vendor signature over the manifest. The device then receives the image in
 * sequential chunks, hashing them as they land in the inactive flash slot.
 * At OTA_END it verifies, in order:
 *   1. integrity   the streamed bytes hash to the manifest SHA-256
 *   2. authenticity the manifest signature verifies against the vendor key
 *   3. anti-rollback the security version does not regress
 * Only then is the image READY. On apply it bumps the stored security
 * version, writes the signed manifest into the slot trailer (which the
 * bootloader re-verifies at boot), and resets.
 *
 * Safety + security gates:
 *   - begin only while the device is idle (STANDBY)
 *   - the running image slot is never erased (recoverable / rollback)
 *   - when PAPR_SEC_REQUIRE_SIGNED_OTA is set (default) an unsigned or
 *     mis-signed image is refused.
 */

typedef enum
{
    PAPR_OTA_IDLE = 0,
    PAPR_OTA_RECEIVING,
    PAPR_OTA_READY,
    PAPR_OTA_ERROR
} papr_ota_state_t;

typedef enum
{
    PAPR_OTA_ERR_NONE = 0,
    PAPR_OTA_ERR_BUSY,        /* device not idle (blower running)        */
    PAPR_OTA_ERR_SIZE,        /* image larger than the staging slot      */
    PAPR_OTA_ERR_SEQUENCE,    /* non-sequential / out-of-range offset     */
    PAPR_OTA_ERR_ALIGN,       /* offset or length not write-aligned       */
    PAPR_OTA_ERR_FLASH,       /* erase / write / read-back failed         */
    PAPR_OTA_ERR_HASH,        /* image SHA-256 mismatch                   */
    PAPR_OTA_ERR_AUTH,        /* manifest signature did not verify        */
    PAPR_OTA_ERR_ROLLBACK,    /* security version regressed               */
    PAPR_OTA_ERR_STATE        /* operation not valid in the current state */
} papr_ota_error_t;

typedef struct
{
    papr_ota_state_t    state;
    papr_ota_error_t    error;
    uint32_t            image_size;
    uint32_t            received;
    uint32_t            next_offset;
    papr_img_manifest_t manifest;     /* from OTA_BEGIN                    */
    papr_sha256_ctx_t   hash;         /* running hash over received bytes */
    papr_secure_t      *sec;          /* shared security context          */
} papr_ota_t;

papr_status_t papr_ota_init(papr_ota_t *o, papr_secure_t *sec);

/* Starts a session from a parsed manifest: validates magic/size, erases the
 * staging slot, begins the running hash. */
papr_ota_error_t papr_ota_begin(papr_ota_t *o,
                                const papr_img_manifest_t *manifest,
                                bool device_idle);

/* Programs one sequential chunk and folds it into the running hash. */
papr_ota_error_t papr_ota_write(papr_ota_t *o,
                                uint32_t offset,
                                const uint8_t *data,
                                uint16_t len);

/* Finalises the hash and runs integrity + authenticity + anti-rollback. */
papr_ota_error_t papr_ota_finish(papr_ota_t *o);

/* From READY: bump the anti-rollback counter, commit the signed manifest to
 * the slot trailer. Caller flushes its ack then reboots. */
papr_ota_error_t papr_ota_apply(papr_ota_t *o);

void papr_ota_abort(papr_ota_t *o);

uint8_t  papr_ota_progress_percent(const papr_ota_t *o);
bool     papr_ota_in_progress(const papr_ota_t *o);

#endif /* PAPR_OTA_H */
