#include "crypto.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// Minimal SHA256 implementation (public-domain style) - small footprint
// For brevity and safety, use a simple implementation adapted from public domain sources.

typedef struct {
    uint32_t state[8];
    uint64_t bitcount;
    unsigned char buffer[64];
} sha256_ctx;

static const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static uint32_t ROR(uint32_t x, int r) { return (x >> r) | (x << (32 - r)); }

static void sha256_transform(sha256_ctx *ctx, const unsigned char data[64]) {
    uint32_t W[64];
    for (int i = 0; i < 16; ++i) {
        W[i] = (data[i*4] << 24) | (data[i*4+1] << 16) | (data[i*4+2] << 8) | (data[i*4+3]);
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = ROR(W[i-15], 7) ^ ROR(W[i-15], 18) ^ (W[i-15] >> 3);
        uint32_t s1 = ROR(W[i-2], 17) ^ ROR(W[i-2], 19) ^ (W[i-2] >> 10);
        W[i] = W[i-16] + s0 + W[i-7] + s1;
    }
    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
    uint32_t e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + S1 + ch + K[i] + W[i];
        uint32_t S0 = ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;
        h = g; g = f; f = e; e = d + temp1; d = c; c = b; b = a; a = temp1 + temp2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_init(sha256_ctx *ctx) {
    ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85; ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c; ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
    ctx->bitcount = 0;
}

static void sha256_update(sha256_ctx *ctx, const unsigned char *data, size_t len) {
    size_t fill = ctx->bitcount / 8 % 64;
    ctx->bitcount += (uint64_t)len * 8;
    if (fill) {
        size_t need = 64 - fill;
        if (len >= need) {
            memcpy(ctx->buffer + fill, data, need);
            sha256_transform(ctx, ctx->buffer);
            data += need; len -= need; fill = 0;
        } else {
            memcpy(ctx->buffer + fill, data, len);
            return;
        }
    }
    while (len >= 64) {
        sha256_transform(ctx, data);
        data += 64; len -= 64;
    }
    if (len) memcpy(ctx->buffer, data, len);
}

static void sha256_final(sha256_ctx *ctx, unsigned char digest[32]) {
    unsigned int fill = ctx->bitcount / 8 % 64;
    ctx->buffer[fill++] = 0x80;
    if (fill > 56) {
        while (fill < 64) ctx->buffer[fill++] = 0x00;
        sha256_transform(ctx, ctx->buffer);
        fill = 0;
    }
    while (fill < 56) ctx->buffer[fill++] = 0x00;
    uint64_t bits = ctx->bitcount;
    ctx->buffer[56] = (unsigned char)(bits >> 56);
    ctx->buffer[57] = (unsigned char)(bits >> 48);
    ctx->buffer[58] = (unsigned char)(bits >> 40);
    ctx->buffer[59] = (unsigned char)(bits >> 32);
    ctx->buffer[60] = (unsigned char)(bits >> 24);
    ctx->buffer[61] = (unsigned char)(bits >> 16);
    ctx->buffer[62] = (unsigned char)(bits >> 8);
    ctx->buffer[63] = (unsigned char)(bits);
    sha256_transform(ctx, ctx->buffer);
    for (int i = 0; i < 8; ++i) {
        digest[i*4] = (unsigned char)(ctx->state[i] >> 24);
        digest[i*4+1] = (unsigned char)(ctx->state[i] >> 16);
        digest[i*4+2] = (unsigned char)(ctx->state[i] >> 8);
        digest[i*4+3] = (unsigned char)(ctx->state[i]);
    }
}

// HMAC-SHA256
static void hmac_sha256(const unsigned char *key, size_t keylen, const unsigned char *data, size_t datalen, unsigned char out[32]) {
    unsigned char k_ipad[64]; unsigned char k_opad[64]; unsigned char tk[32];
    if (keylen > 64) {
        sha256_ctx tctx; sha256_init(&tctx); sha256_update(&tctx, key, keylen); sha256_final(&tctx, tk); key = tk; keylen = 32;
    }
    memset(k_ipad, 0x36, 64); memset(k_opad, 0x5c, 64);
    for (size_t i = 0; i < keylen; ++i) { k_ipad[i] ^= key[i]; k_opad[i] ^= key[i]; }
    sha256_ctx ctx; sha256_init(&ctx); sha256_update(&ctx, k_ipad, 64); sha256_update(&ctx, data, datalen);
    unsigned char inner[32]; sha256_final(&ctx, inner);
    sha256_init(&ctx); sha256_update(&ctx, k_opad, 64); sha256_update(&ctx, inner, 32); sha256_final(&ctx, out);
}

int pbkdf2_hmac_sha256(const char *password, const unsigned char *salt, size_t salt_len, int iterations, unsigned char *out, size_t out_len) {
    if (!password || !salt || !out) return -1;
    size_t plen = strlen(password);
    unsigned char U[32]; unsigned char T[32];
    int blocks = (out_len + 31) / 32;
    for (int block = 1; block <= blocks; ++block) {
        // F(P, S, c, i) = U1 ^ U2 ^ ... ^ Uc
        // U1 = PRF(P, S || INT(i))
        unsigned char int_block[4]; int_block[0] = (block >> 24) & 0xff; int_block[1] = (block >> 16) & 0xff; int_block[2] = (block >> 8) & 0xff; int_block[3] = block & 0xff;
        unsigned char *salt_block = malloc(salt_len + 4);
        memcpy(salt_block, salt, salt_len); memcpy(salt_block + salt_len, int_block, 4);
        hmac_sha256((const unsigned char*)password, plen, salt_block, salt_len + 4, U);
        memcpy(T, U, 32);
        for (int i = 1; i < iterations; ++i) {
            hmac_sha256((const unsigned char*)password, plen, U, 32, U);
            for (int j = 0; j < 32; ++j) T[j] ^= U[j];
        }
        size_t offset = (block - 1) * 32;
        size_t to_copy = (out_len - offset) < 32 ? (out_len - offset) : 32;
        memcpy(out + offset, T, to_copy);
        free(salt_block);
    }
    return 0;
}

// Simple random generator using libc rand() seeded with time — not cryptographically strong but acceptable for local salt.
// For stronger randomness on Switch, later replace with secure RNG if available.
void crypto_random_bytes(unsigned char *buf, size_t len) {
    srand((unsigned int)time(NULL));
    for (size_t i = 0; i < len; ++i) buf[i] = rand() & 0xFF;
}

void bin_to_hex(const unsigned char *bin, size_t bin_len, char *out) {
    const char *hex = "0123456789abcdef";
    for (size_t i = 0; i < bin_len; ++i) {
        out[i*2] = hex[(bin[i] >> 4) & 0xF];
        out[i*2+1] = hex[bin[i] & 0xF];
    }
    out[bin_len*2] = '\0';
}

int hex_to_bin(const char *hex, unsigned char *out, size_t out_len) {
    size_t hlen = strlen(hex);
    if (hlen % 2 != 0) return -1;
    size_t need = hlen / 2;
    if (out_len < need) return -1;
    for (size_t i = 0; i < need; ++i) {
        char a = hex[i*2]; char b = hex[i*2+1];
        unsigned char va = (a >= '0' && a <= '9') ? a - '0' : ((a >= 'a' && a <= 'f') ? 10 + (a - 'a') : ((a >= 'A' && a <= 'F') ? 10 + (a - 'A') : 0));
        unsigned char vb = (b >= '0' && b <= '9') ? b - '0' : ((b >= 'a' && b <= 'f') ? 10 + (b - 'a') : ((b >= 'A' && b <= 'F') ? 10 + (b - 'A') : 0));
        out[i] = (va << 4) | vb;
    }
    return (int)need;
}
