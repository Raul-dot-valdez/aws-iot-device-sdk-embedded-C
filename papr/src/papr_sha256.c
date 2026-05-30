#include "papr_sha256.h"

#include <string.h>

static const uint32_t K[64] = {
    0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,
    0x923f82a4U,0xab1c5ed5U,0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,
    0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,0xe49b69c1U,0xefbe4786U,
    0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
    0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,
    0x06ca6351U,0x14292967U,0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,
    0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,0xa2bfe8a1U,0xa81a664bU,
    0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
    0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,
    0x5b9cca4fU,0x682e6ff3U,0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,
    0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U
};

static uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32U - n)); }

static void sha256_block(papr_sha256_ctx_t *c, const uint8_t *p)
{
    uint32_t w[64];
    for (uint32_t i = 0U; i < 16U; ++i)
    {
        w[i] = ((uint32_t)p[i * 4U] << 24) | ((uint32_t)p[i * 4U + 1U] << 16) |
               ((uint32_t)p[i * 4U + 2U] << 8) | (uint32_t)p[i * 4U + 3U];
    }
    for (uint32_t i = 16U; i < 64U; ++i)
    {
        uint32_t s0 = rotr(w[i - 15U], 7) ^ rotr(w[i - 15U], 18) ^ (w[i - 15U] >> 3);
        uint32_t s1 = rotr(w[i - 2U], 17) ^ rotr(w[i - 2U], 19) ^ (w[i - 2U] >> 10);
        w[i] = w[i - 16U] + s0 + w[i - 7U] + s1;
    }

    uint32_t a = c->state[0], b = c->state[1], cc = c->state[2], d = c->state[3];
    uint32_t e = c->state[4], f = c->state[5], g = c->state[6], h = c->state[7];

    for (uint32_t i = 0U; i < 64U; ++i)
    {
        uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1; d = cc; cc = b; b = a; a = t1 + t2;
    }

    c->state[0] += a; c->state[1] += b; c->state[2] += cc; c->state[3] += d;
    c->state[4] += e; c->state[5] += f; c->state[6] += g; c->state[7] += h;
}

void papr_sha256_init(papr_sha256_ctx_t *c)
{
    c->state[0] = 0x6a09e667U; c->state[1] = 0xbb67ae85U;
    c->state[2] = 0x3c6ef372U; c->state[3] = 0xa54ff53aU;
    c->state[4] = 0x510e527fU; c->state[5] = 0x9b05688cU;
    c->state[6] = 0x1f83d9abU; c->state[7] = 0x5be0cd19U;
    c->bitlen = 0U;
    c->buflen = 0U;
}

void papr_sha256_update(papr_sha256_ctx_t *c, const uint8_t *data, size_t len)
{
    for (size_t i = 0U; i < len; ++i)
    {
        c->buf[c->buflen++] = data[i];
        if (c->buflen == PAPR_SHA256_BLOCK_LEN)
        {
            sha256_block(c, c->buf);
            c->bitlen += 512U;
            c->buflen = 0U;
        }
    }
}

void papr_sha256_final(papr_sha256_ctx_t *c, uint8_t out[PAPR_SHA256_DIGEST_LEN])
{
    uint64_t total_bits = c->bitlen + (uint64_t)c->buflen * 8U;
    uint32_t i = c->buflen;

    c->buf[i++] = 0x80U;
    if (i > 56U)
    {
        while (i < PAPR_SHA256_BLOCK_LEN) { c->buf[i++] = 0U; }
        sha256_block(c, c->buf);
        i = 0U;
    }
    while (i < 56U) { c->buf[i++] = 0U; }
    for (int8_t s = 56; s >= 0; s -= 8)
    {
        c->buf[i++] = (uint8_t)(total_bits >> (uint32_t)s);
    }
    sha256_block(c, c->buf);

    for (uint32_t j = 0U; j < 8U; ++j)
    {
        out[j * 4U]      = (uint8_t)(c->state[j] >> 24);
        out[j * 4U + 1U] = (uint8_t)(c->state[j] >> 16);
        out[j * 4U + 2U] = (uint8_t)(c->state[j] >> 8);
        out[j * 4U + 3U] = (uint8_t)(c->state[j]);
    }
}

void papr_sha256(const uint8_t *data, size_t len, uint8_t out[PAPR_SHA256_DIGEST_LEN])
{
    papr_sha256_ctx_t c;
    papr_sha256_init(&c);
    papr_sha256_update(&c, data, len);
    papr_sha256_final(&c, out);
}

void papr_hmac_sha256(const uint8_t *key, size_t key_len,
                      const uint8_t *msg, size_t msg_len,
                      uint8_t out[PAPR_SHA256_DIGEST_LEN])
{
    uint8_t k[PAPR_SHA256_BLOCK_LEN];
    uint8_t ipad[PAPR_SHA256_BLOCK_LEN];
    uint8_t opad[PAPR_SHA256_BLOCK_LEN];
    uint8_t inner[PAPR_SHA256_DIGEST_LEN];

    memset(k, 0, sizeof(k));
    if (key_len > PAPR_SHA256_BLOCK_LEN)
    {
        papr_sha256(key, key_len, k);   /* fits in 32 < 64 */
    }
    else
    {
        memcpy(k, key, key_len);
    }

    for (uint32_t i = 0U; i < PAPR_SHA256_BLOCK_LEN; ++i)
    {
        ipad[i] = (uint8_t)(k[i] ^ 0x36U);
        opad[i] = (uint8_t)(k[i] ^ 0x5cU);
    }

    papr_sha256_ctx_t c;
    papr_sha256_init(&c);
    papr_sha256_update(&c, ipad, sizeof(ipad));
    papr_sha256_update(&c, msg, msg_len);
    papr_sha256_final(&c, inner);

    papr_sha256_init(&c);
    papr_sha256_update(&c, opad, sizeof(opad));
    papr_sha256_update(&c, inner, sizeof(inner));
    papr_sha256_final(&c, out);
}

bool papr_ct_equal(const uint8_t *a, const uint8_t *b, size_t len)
{
    uint8_t diff = 0U;
    for (size_t i = 0U; i < len; ++i) { diff |= (uint8_t)(a[i] ^ b[i]); }
    return diff == 0U;
}

bool papr_sha256_selftest(void)
{
    static const uint8_t want_abc[32] = {
        0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
        0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad
    };
    uint8_t d[32];
    papr_sha256((const uint8_t *)"abc", 3U, d);
    if (!papr_ct_equal(d, want_abc, 32U)) { return false; }

    static const uint8_t want_hmac[32] = {
        0x5b,0xdc,0xc1,0x46,0xbf,0x60,0x75,0x4e,0x6a,0x04,0x24,0x26,0x08,0x95,0x75,0xc7,
        0x5a,0x00,0x3f,0x08,0x9d,0x27,0x39,0x83,0x9d,0xec,0x58,0xb9,0x64,0xec,0x38,0x43
    };
    papr_hmac_sha256((const uint8_t *)"Jefe", 4U,
                     (const uint8_t *)"what do ya want for nothing?", 28U, d);
    return papr_ct_equal(d, want_hmac, 32U);
}
