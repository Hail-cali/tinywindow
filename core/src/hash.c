#include "nanofilter.h"
#include <string.h>

/* ── MurmurHash3_x86_128 ───────────────────────────────────────── *
 * Reference: https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
 * Adapted to C with identical bit output.
 * ─────────────────────────────────────────────────────────────────── */

static inline uint32_t rotl32(uint32_t x, int8_t r) {
    return (x << r) | (x >> (32 - r));
}

static inline uint32_t fmix32(uint32_t h) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

static inline uint32_t getblock32(const uint32_t *p, int i) {
    uint32_t val;
    memcpy(&val, &p[i], sizeof(val));
    return val;
}

void nf_murmurhash3_x86_128(const void *key, size_t len,
                             uint32_t seed, void *out) {
    const uint8_t *data = (const uint8_t *)key;
    const int nblocks = (int)(len / 16);

    uint32_t h1 = seed;
    uint32_t h2 = seed;
    uint32_t h3 = seed;
    uint32_t h4 = seed;

    const uint32_t c1 = 0x239b961b;
    const uint32_t c2 = 0xab0e9789;
    const uint32_t c3 = 0x38b34ae5;
    const uint32_t c4 = 0xa1e38b93;

    /* body */
    const uint32_t *blocks = (const uint32_t *)(data + (size_t)nblocks * 16);

    for (int i = -nblocks; i; i++) {
        uint32_t k1 = getblock32(blocks, i * 4 + 0);
        uint32_t k2 = getblock32(blocks, i * 4 + 1);
        uint32_t k3 = getblock32(blocks, i * 4 + 2);
        uint32_t k4 = getblock32(blocks, i * 4 + 3);

        k1 *= c1; k1 = rotl32(k1, 15); k1 *= c2; h1 ^= k1;
        h1 = rotl32(h1, 19); h1 += h2; h1 = h1 * 5 + 0x561ccd1b;

        k2 *= c2; k2 = rotl32(k2, 16); k2 *= c3; h2 ^= k2;
        h2 = rotl32(h2, 17); h2 += h3; h2 = h2 * 5 + 0x0bcaa747;

        k3 *= c3; k3 = rotl32(k3, 17); k3 *= c4; h3 ^= k3;
        h3 = rotl32(h3, 15); h3 += h4; h3 = h3 * 5 + 0x96cd1c35;

        k4 *= c4; k4 = rotl32(k4, 18); k4 *= c1; h4 ^= k4;
        h4 = rotl32(h4, 13); h4 += h1; h4 = h4 * 5 + 0x32ac3b17;
    }

    /* tail */
    const uint8_t *tail = (const uint8_t *)(data + (size_t)nblocks * 16);

    uint32_t k1 = 0;
    uint32_t k2 = 0;
    uint32_t k3 = 0;
    uint32_t k4 = 0;

    switch (len & 15) {
    case 15: k4 ^= (uint32_t)tail[14] << 16; /* fallthrough */
    case 14: k4 ^= (uint32_t)tail[13] << 8;  /* fallthrough */
    case 13: k4 ^= (uint32_t)tail[12] << 0;
             k4 *= c4; k4 = rotl32(k4, 18); k4 *= c1; h4 ^= k4;
             /* fallthrough */
    case 12: k3 ^= (uint32_t)tail[11] << 24; /* fallthrough */
    case 11: k3 ^= (uint32_t)tail[10] << 16; /* fallthrough */
    case 10: k3 ^= (uint32_t)tail[ 9] << 8;  /* fallthrough */
    case  9: k3 ^= (uint32_t)tail[ 8] << 0;
             k3 *= c3; k3 = rotl32(k3, 17); k3 *= c4; h3 ^= k3;
             /* fallthrough */
    case  8: k2 ^= (uint32_t)tail[ 7] << 24; /* fallthrough */
    case  7: k2 ^= (uint32_t)tail[ 6] << 16; /* fallthrough */
    case  6: k2 ^= (uint32_t)tail[ 5] << 8;  /* fallthrough */
    case  5: k2 ^= (uint32_t)tail[ 4] << 0;
             k2 *= c2; k2 = rotl32(k2, 16); k2 *= c3; h2 ^= k2;
             /* fallthrough */
    case  4: k1 ^= (uint32_t)tail[ 3] << 24; /* fallthrough */
    case  3: k1 ^= (uint32_t)tail[ 2] << 16; /* fallthrough */
    case  2: k1 ^= (uint32_t)tail[ 1] << 8;  /* fallthrough */
    case  1: k1 ^= (uint32_t)tail[ 0] << 0;
             k1 *= c1; k1 = rotl32(k1, 15); k1 *= c2; h1 ^= k1;
    }

    /* finalization */
    h1 ^= (uint32_t)len; h2 ^= (uint32_t)len;
    h3 ^= (uint32_t)len; h4 ^= (uint32_t)len;

    h1 += h2; h1 += h3; h1 += h4;
    h2 += h1; h3 += h1; h4 += h1;

    h1 = fmix32(h1);
    h2 = fmix32(h2);
    h3 = fmix32(h3);
    h4 = fmix32(h4);

    h1 += h2; h1 += h3; h1 += h4;
    h2 += h1; h3 += h1; h4 += h1;

    uint32_t *result = (uint32_t *)out;
    result[0] = h1;
    result[1] = h2;
    result[2] = h3;
    result[3] = h4;
}

void nf_hash128(const char *key, uint32_t seed,
                uint64_t *h1_out, uint64_t *h2_out) {
    uint32_t out[4];
    nf_murmurhash3_x86_128(key, strlen(key), seed, out);
    *h1_out = ((uint64_t)out[1] << 32) | (uint64_t)out[0];
    *h2_out = ((uint64_t)out[3] << 32) | (uint64_t)out[2];
}
