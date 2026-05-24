#ifndef PAPR_OTA_H
#define PAPR_OTA_H

#include "papr_config.h"
#include "papr_types.h"
#include "papr_version.h"

/* Over-the-air firmware update receiver.
 *
 * Transport-agnostic: the BLE module (or any other link) feeds image bytes in
 * through papr_ota_write(); the module stages them in the inactive flash slot
 * via the HAL, verifies a CRC-32 over the whole image, and on apply asks the
 * HAL to set the boot flag and reset. A separate bootloader (documented in
 * ARCHITECTURE.md, not part of this build) validates and runs the staged image
 * with rollback on boot failure.
 *
 * Safety: papr_ota_begin() must be told whether the device is idle. The
 * controller only passes true in STANDBY, so an update can never start while
 * the worker is breathing through the unit.
 *
 * Session lifecycle:
 *   begin  -> RECEIVING        (slot erased, header recorded)
 *   write* -> RECEIVING        (sequential chunks)
 *   finish -> READY | ERROR    (CRC verified)
 *   apply  -> commit + reboot  (only from READY)
 *   abort  -> IDLE             (any time)
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
    PAPR_OTA_ERR_CRC,         /* whole-image CRC-32 mismatch              */
    PAPR_OTA_ERR_VERSION,     /* image older than running fw (downgrade)  */
    PAPR_OTA_ERR_STATE        /* operation not valid in the current state */
} papr_ota_error_t;

typedef struct
{
    papr_ota_state_t  state;
    papr_ota_error_t  error;
    uint32_t          image_size;     /* total bytes the app declared     */
    uint32_t          received;       /* bytes written so far             */
    uint32_t          next_offset;    /* expected offset of next chunk    */
    uint32_t          expected_crc32; /* CRC-32 declared in OTA_BEGIN     */
    papr_fw_version_t incoming;       /* version declared in OTA_BEGIN    */
} papr_ota_t;

papr_status_t papr_ota_init(papr_ota_t *o);

/* Starts a session: validates size/version, erases the staging slot. */
papr_ota_error_t papr_ota_begin(papr_ota_t *o,
                                uint32_t image_size,
                                uint32_t image_crc32,
                                papr_fw_version_t incoming,
                                bool device_idle);

/* Programs one sequential chunk. offset must equal the running next_offset. */
papr_ota_error_t papr_ota_write(papr_ota_t *o,
                                uint32_t offset,
                                const uint8_t *data,
                                uint16_t len);

/* Reads the staged image back, recomputes CRC-32, transitions to READY. */
papr_ota_error_t papr_ota_finish(papr_ota_t *o);

/* From READY: commit boot metadata. Returns the error if not ready; on
 * success the caller should flush any acknowledgement and then reboot. */
papr_ota_error_t papr_ota_apply(papr_ota_t *o);

void papr_ota_abort(papr_ota_t *o);

uint8_t  papr_ota_progress_percent(const papr_ota_t *o);
bool     papr_ota_in_progress(const papr_ota_t *o);   /* true while RECEIVING */

#endif /* PAPR_OTA_H */
