#ifndef PAPR_SECURE_H
#define PAPR_SECURE_H

#include "papr_config.h"
#include "papr_types.h"
#include "../boot/boot_shared.h"

/* Cybersecurity services. Designed against the controls common to the more
 * stringent IoT / connected-device regimes:
 *
 *   ETSI EN 303 645 / EN 18031   no universal default credentials, secure
 *                                update, secure communication, minimise
 *                                exposed surfaces, protect security params
 *   NIST IR 8259 / IR 8425       device identity, secure update, data
 *                                protection, access control, logging
 *   IEC 62443-4-2 (CR1/CR3/CR7)  identification & auth, integrity, resource
 *                                availability
 *   FDA premarket cybersecurity  authenticity, integrity, anti-rollback,
 *                                secure boot, audit
 *
 * Concretely the module provides:
 *   1. Authenticated control     a BLE central must prove knowledge of the
 *                                per-device key (HMAC-SHA256 challenge /
 *                                response with a random nonce + counter)
 *                                before any state-changing command is honoured.
 *                                Read-only telemetry stays open.
 *   2. Authentic, anti-rollback  OTA images are accepted only if the streamed
 *      firmware update           bytes hash to the manifest SHA-256, the
 *                                manifest signature verifies against the
 *                                vendor key, and the security version does not
 *                                regress.
 *   3. Secure-boot continuity    the same manifest the bootloader verifies at
 *                                boot, so the chain of trust is unbroken.
 *   4. Audit + lockout           failed auth / rejected updates are counted;
 *                                repeated auth failures lock out new attempts.
 */

#define PAPR_SEC_NONCE_LEN   16U
#define PAPR_SEC_TAG_LEN     16U   /* truncated HMAC tag exchanged on the wire */
#define PAPR_SEC_KEY_LEN     32U

/* Provisioned key identifiers (papr_hal_sec_key_read). */
typedef enum
{
    PAPR_KEY_SESSION = 0,   /* per-device secret for BLE session auth        */
    PAPR_KEY_VENDOR  = 1    /* vendor key for image-signature verification    */
} papr_key_id_t;

typedef enum
{
    PAPR_SEC_OK = 0,
    PAPR_SEC_ERR_HASH,        /* image bytes do not match manifest SHA-256   */
    PAPR_SEC_ERR_SIGNATURE,   /* manifest signature did not verify           */
    PAPR_SEC_ERR_ROLLBACK,    /* sec_version regressed                       */
    PAPR_SEC_ERR_MAGIC,       /* manifest malformed                          */
    PAPR_SEC_ERR_SELFTEST     /* crypto self-test failed; refuse to trust     */
} papr_sec_result_t;

typedef enum
{
    PAPR_AUTH_IDLE = 0,       /* no challenge outstanding                    */
    PAPR_AUTH_CHALLENGED,     /* nonce issued, awaiting response             */
    PAPR_AUTH_OPEN,           /* session authenticated                       */
    PAPR_AUTH_LOCKED          /* too many failures, temporarily refusing     */
} papr_auth_state_t;

typedef struct
{
    papr_auth_state_t auth_state;
    uint8_t  nonce[PAPR_SEC_NONCE_LEN];
    uint32_t challenge_counter;     /* monotonic, anti-replay across boots-ish */
    uint32_t challenge_issued_ms;
    uint8_t  fail_count;
    uint32_t lock_until_ms;
    bool     crypto_ok;             /* self-test passed                       */

    /* Audit counters (read by the BLE SEC_STATUS / factory log). */
    uint16_t audit_auth_fail;
    uint16_t audit_ota_reject;
    uint8_t  last_sec_result;       /* papr_sec_result_t of last image check  */
} papr_secure_t;

papr_status_t papr_secure_init(papr_secure_t *s);

/* ---- Authenticated control (BLE) ----------------------------------------- */

/* True only when both enforcement is enabled AND a session is authenticated.
 * When PAPR_SEC_REQUIRE_AUTH == 0 this always returns true (open mode). */
bool papr_secure_control_allowed(const papr_secure_t *s);

/* Issues a challenge: fills nonce_out (PAPR_SEC_NONCE_LEN) and the 4-byte
 * counter, moves to CHALLENGED. Returns false if currently locked out. */
bool papr_secure_auth_begin(papr_secure_t *s, uint32_t now_ms,
                            uint8_t nonce_out[PAPR_SEC_NONCE_LEN],
                            uint32_t *counter_out);

/* Verifies the central's response tag (PAPR_SEC_TAG_LEN). On success the
 * session becomes OPEN. On failure increments the fail counter and may lock
 * out. Constant-time tag comparison. */
bool papr_secure_auth_verify(papr_secure_t *s, uint32_t now_ms,
                             const uint8_t tag[PAPR_SEC_TAG_LEN]);

/* Drop the authenticated session (call on BLE disconnect / timeout). */
void papr_secure_session_close(papr_secure_t *s);

/* ---- Firmware-image authenticity + anti-rollback ------------------------- */

/* Verifies a fully-staged image: computed_sha must equal the manifest hash,
 * the manifest signature must verify against the vendor key, and
 * manifest->sec_version must be >= current_sec_version. */
papr_sec_result_t papr_secure_verify_image(papr_secure_t *s,
                                           const papr_img_manifest_t *m,
                                           const uint8_t computed_sha[PAPR_SHA256_LEN],
                                           uint32_t current_sec_version);

/* ---- Status -------------------------------------------------------------- */
void papr_secure_status(const papr_secure_t *s,
                        uint8_t *out_auth_state,
                        uint8_t *out_flags,
                        uint16_t *out_auth_fail,
                        uint16_t *out_ota_reject);

/* Status flag bits returned in out_flags. */
#define PAPR_SEC_FLAG_CRYPTO_OK     0x01U
#define PAPR_SEC_FLAG_AUTH_REQUIRED 0x02U
#define PAPR_SEC_FLAG_SIGNED_OTA    0x04U
#define PAPR_SEC_FLAG_DEBUG_LOCKED  0x08U

#endif /* PAPR_SECURE_H */
