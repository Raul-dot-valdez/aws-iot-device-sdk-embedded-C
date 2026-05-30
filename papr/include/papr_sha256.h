#ifndef PAPR_SHA256_H
#define PAPR_SHA256_H

#include "papr_types.h"

/* Self-contained SHA-256 and HMAC-SHA256. No heap, no libc beyond memcpy.
 * Used for OTA image integrity, the BLE session-auth challenge/response, and
 * (in the reference HAL) the firmware-image authenticity tag.
 *
 * Verified against the standard test vectors:
 *   SHA-256("abc") = ba7816bf 8f01cfea 414140de 5dae2223
 *                    b00361a3 96177a9c b410ff61 f20015ad
 *   HMAC-SHA256(key="Jefe", "what do ya want for nothing?")
 *                  = 5bdcc146 bf60754e 6a042426 089575c7
 *                    5a003f08 9d273983 9dec58b9 64ec3843
 */

#define PAPR_SHA256_DIGEST_LEN  32U
#define PAPR_SHA256_BLOCK_LEN   64U

typedef struct
{
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t  buf[PAPR_SHA256_BLOCK_LEN];
    uint32_t buflen;
} papr_sha256_ctx_t;

void papr_sha256_init(papr_sha256_ctx_t *c);
void papr_sha256_update(papr_sha256_ctx_t *c, const uint8_t *data, size_t len);
void papr_sha256_final(papr_sha256_ctx_t *c, uint8_t out[PAPR_SHA256_DIGEST_LEN]);

/* One-shot convenience. */
void papr_sha256(const uint8_t *data, size_t len,
                 uint8_t out[PAPR_SHA256_DIGEST_LEN]);

/* HMAC-SHA256 over (key, msg). out is the full 32-byte tag. */
void papr_hmac_sha256(const uint8_t *key, size_t key_len,
                      const uint8_t *msg, size_t msg_len,
                      uint8_t out[PAPR_SHA256_DIGEST_LEN]);

/* Constant-time compare of two equal-length buffers; returns true if equal.
 * Used for tag/MAC checks so timing does not leak how many bytes matched. */
bool papr_ct_equal(const uint8_t *a, const uint8_t *b, size_t len);

/* Runs the built-in known-answer self-test. Returns true on success. Called
 * once at boot before any security operation is trusted. */
bool papr_sha256_selftest(void);

#endif /* PAPR_SHA256_H */
