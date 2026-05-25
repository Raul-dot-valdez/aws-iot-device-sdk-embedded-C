#include "papr_secure.h"
#include "papr_hal.h"
#include "papr_sha256.h"

#include <string.h>

papr_status_t papr_secure_init(papr_secure_t *s)
{
    if (s == NULL) { return PAPR_ERR_PARAM; }
    memset(s, 0, sizeof(*s));
    s->auth_state = PAPR_AUTH_IDLE;
    /* Refuse to trust any security operation if the crypto primitives do not
     * pass their known-answer self-test (a tampered or mis-built image). */
    s->crypto_ok = papr_sha256_selftest();
    return s->crypto_ok ? PAPR_OK : PAPR_ERR_HW;
}

/* ---- Authenticated control ----------------------------------------------- */

bool papr_secure_control_allowed(const papr_secure_t *s)
{
    if (s == NULL) { return false; }
#if PAPR_SEC_REQUIRE_AUTH
    return s->crypto_ok && (s->auth_state == PAPR_AUTH_OPEN);
#else
    (void)s;
    return true;
#endif
}

/* tag = HMAC-SHA256(session_key, nonce(16) || counter_le(4))[0:TAG_LEN] */
static bool compute_expected_tag(const papr_secure_t *s, uint8_t out[PAPR_SEC_TAG_LEN])
{
    uint8_t key[PAPR_SEC_KEY_LEN];
    if (papr_hal_sec_key_read(PAPR_KEY_SESSION, key, sizeof(key)) != PAPR_OK)
    {
        return false;
    }
    uint8_t msg[PAPR_SEC_NONCE_LEN + 4U];
    memcpy(msg, s->nonce, PAPR_SEC_NONCE_LEN);
    msg[PAPR_SEC_NONCE_LEN + 0U] = (uint8_t)s->challenge_counter;
    msg[PAPR_SEC_NONCE_LEN + 1U] = (uint8_t)(s->challenge_counter >> 8);
    msg[PAPR_SEC_NONCE_LEN + 2U] = (uint8_t)(s->challenge_counter >> 16);
    msg[PAPR_SEC_NONCE_LEN + 3U] = (uint8_t)(s->challenge_counter >> 24);

    uint8_t full[PAPR_SHA256_DIGEST_LEN];
    papr_hmac_sha256(key, sizeof(key), msg, sizeof(msg), full);
    memcpy(out, full, PAPR_SEC_TAG_LEN);

    /* Wipe the key copy from the stack. */
    memset(key, 0, sizeof(key));
    return true;
}

bool papr_secure_auth_begin(papr_secure_t *s, uint32_t now_ms,
                            uint8_t nonce_out[PAPR_SEC_NONCE_LEN],
                            uint32_t *counter_out)
{
    if (s == NULL || nonce_out == NULL || !s->crypto_ok) { return false; }

    if (s->auth_state == PAPR_AUTH_LOCKED)
    {
        if ((int32_t)(now_ms - s->lock_until_ms) < 0) { return false; }
        s->auth_state = PAPR_AUTH_IDLE;
        s->fail_count = 0U;
    }

    if (papr_hal_rng(s->nonce, PAPR_SEC_NONCE_LEN) != PAPR_OK) { return false; }
    s->challenge_counter++;
    s->challenge_issued_ms = now_ms;
    s->auth_state = PAPR_AUTH_CHALLENGED;

    memcpy(nonce_out, s->nonce, PAPR_SEC_NONCE_LEN);
    if (counter_out != NULL) { *counter_out = s->challenge_counter; }
    return true;
}

bool papr_secure_auth_verify(papr_secure_t *s, uint32_t now_ms,
                             const uint8_t tag[PAPR_SEC_TAG_LEN])
{
    if (s == NULL || tag == NULL || !s->crypto_ok) { return false; }
    if (s->auth_state != PAPR_AUTH_CHALLENGED) { return false; }
    if ((uint32_t)(now_ms - s->challenge_issued_ms) > PAPR_SEC_CHALLENGE_TTL_MS)
    {
        s->auth_state = PAPR_AUTH_IDLE;   /* stale challenge */
        return false;
    }

    uint8_t expected[PAPR_SEC_TAG_LEN];
    if (!compute_expected_tag(s, expected)) { return false; }

    if (papr_ct_equal(tag, expected, PAPR_SEC_TAG_LEN))
    {
        s->auth_state = PAPR_AUTH_OPEN;
        s->fail_count = 0U;
        return true;
    }

    /* Failure: count it, lock out after the threshold. */
    s->auth_state = PAPR_AUTH_IDLE;
    if (s->audit_auth_fail < 0xFFFFU) { s->audit_auth_fail++; }
    if (s->fail_count < 0xFFU) { s->fail_count++; }
    if (s->fail_count >= PAPR_SEC_AUTH_MAX_FAILS)
    {
        s->auth_state  = PAPR_AUTH_LOCKED;
        s->lock_until_ms = now_ms + PAPR_SEC_AUTH_LOCKOUT_MS;
    }
    return false;
}

void papr_secure_session_close(papr_secure_t *s)
{
    if (s == NULL) { return; }
    if (s->auth_state == PAPR_AUTH_OPEN || s->auth_state == PAPR_AUTH_CHALLENGED)
    {
        s->auth_state = PAPR_AUTH_IDLE;
    }
    memset(s->nonce, 0, sizeof(s->nonce));
}

/* ---- Image authenticity + anti-rollback ---------------------------------- */

papr_sec_result_t papr_secure_verify_image(papr_secure_t *s,
                                           const papr_img_manifest_t *m,
                                           const uint8_t computed_sha[PAPR_SHA256_LEN],
                                           uint32_t current_sec_version)
{
    if (s == NULL || m == NULL || computed_sha == NULL)
    {
        return PAPR_SEC_ERR_MAGIC;
    }
    if (!s->crypto_ok) { s->last_sec_result = PAPR_SEC_ERR_SELFTEST; return PAPR_SEC_ERR_SELFTEST; }
    if (m->magic != PAPR_IMG_MAGIC) { goto reject_magic; }

    /* 1. integrity: the streamed bytes must hash to the manifest digest. */
    if (!papr_ct_equal(computed_sha, m->sha256, PAPR_SHA256_LEN))
    {
        s->last_sec_result = PAPR_SEC_ERR_HASH;
        if (s->audit_ota_reject < 0xFFFFU) { s->audit_ota_reject++; }
        return PAPR_SEC_ERR_HASH;
    }

    /* 2. authenticity: the vendor signature over the manifest digest. We sign
     *    a hash of the manifest header up to and including sha256, so the
     *    declared size / version / fw fields are covered too. */
    uint8_t mh[PAPR_SHA256_LEN];
    const size_t signed_len = offsetof(papr_img_manifest_t, signature);
    papr_sha256((const uint8_t *)m, signed_len, mh);
    if (!papr_hal_sec_verify(mh, m->signature, m->sig_len))
    {
        s->last_sec_result = PAPR_SEC_ERR_SIGNATURE;
        if (s->audit_ota_reject < 0xFFFFU) { s->audit_ota_reject++; }
        return PAPR_SEC_ERR_SIGNATURE;
    }

    /* 3. anti-rollback: never accept an image older than what we've run. */
    if (m->sec_version < current_sec_version)
    {
        s->last_sec_result = PAPR_SEC_ERR_ROLLBACK;
        if (s->audit_ota_reject < 0xFFFFU) { s->audit_ota_reject++; }
        return PAPR_SEC_ERR_ROLLBACK;
    }

    s->last_sec_result = PAPR_SEC_OK;
    return PAPR_SEC_OK;

reject_magic:
    s->last_sec_result = PAPR_SEC_ERR_MAGIC;
    if (s->audit_ota_reject < 0xFFFFU) { s->audit_ota_reject++; }
    return PAPR_SEC_ERR_MAGIC;
}

void papr_secure_status(const papr_secure_t *s,
                        uint8_t *out_auth_state,
                        uint8_t *out_flags,
                        uint16_t *out_auth_fail,
                        uint16_t *out_ota_reject)
{
    if (s == NULL) { return; }
    if (out_auth_state) { *out_auth_state = (uint8_t)s->auth_state; }
    if (out_flags)
    {
        uint8_t f = 0U;
        if (s->crypto_ok)            { f |= PAPR_SEC_FLAG_CRYPTO_OK; }
#if PAPR_SEC_REQUIRE_AUTH
        f |= PAPR_SEC_FLAG_AUTH_REQUIRED;
#endif
#if PAPR_SEC_REQUIRE_SIGNED_OTA
        f |= PAPR_SEC_FLAG_SIGNED_OTA;
#endif
        if (papr_hal_debug_locked())  { f |= PAPR_SEC_FLAG_DEBUG_LOCKED; }
        *out_flags = f;
    }
    if (out_auth_fail)  { *out_auth_fail  = s->audit_auth_fail; }
    if (out_ota_reject) { *out_ota_reject = s->audit_ota_reject; }
}
