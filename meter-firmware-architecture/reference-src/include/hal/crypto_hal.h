/*
 * Crypto HAL - secure-element / PKCS#11-backed operations.
 *
 * Backs mTLS client auth, signed revenue records, signed audit log, and OTA
 * image verification (06-security-architecture.md). Private keys live in the
 * secure element and are used, never exported.
 */
#ifndef CRYPTO_HAL_H_
#define CRYPTO_HAL_H_

#include "../meter_types.h"

struct CryptoInterface;

/* Sign a canonical payload with the device key held in the secure element.
 * Used for per-record revenue signing (08-data-model-shadow.md, "sig"). */
typedef MeterStatus_t ( * CryptoSign_t )( struct CryptoInterface * pCtx,
                                          const uint8_t * pData, size_t dataLen,
                                          uint8_t * pSig, size_t * pSigLen );

/* Verify a signature against a named/trusted public key (e.g. OTA root). */
typedef MeterStatus_t ( * CryptoVerify_t )( struct CryptoInterface * pCtx,
                                            const char * pKeyLabel,
                                            const uint8_t * pData, size_t dataLen,
                                            const uint8_t * pSig, size_t sigLen );

/* SHA-256 (or configured) digest for image/record integrity. */
typedef MeterStatus_t ( * CryptoDigest_t )( struct CryptoInterface * pCtx,
                                            const uint8_t * pData, size_t dataLen,
                                            uint8_t * pDigest, size_t * pDigestLen );

/* Zeroize secret material (decommissioning, 07-ota-and-lifecycle.md section 4). */
typedef MeterStatus_t ( * CryptoZeroize_t )( struct CryptoInterface * pCtx );

typedef struct CryptoInterface
{
    void * pImplCtx;
    CryptoSign_t    sign;
    CryptoVerify_t  verify;
    CryptoDigest_t  digest;
    CryptoZeroize_t zeroize;
} CryptoInterface_t;

#endif /* CRYPTO_HAL_H_ */
