#ifndef HELLO_CRYPTO_H
#define HELLO_CRYPTO_H

#include <stddef.h>

// Derive key using PBKDF2-HMAC-SHA256
// password: input password
// salt: input salt
// iterations: iteration count
// out: output buffer (derived key)
// out_len: length of derived key
int pbkdf2_hmac_sha256(const char *password, const unsigned char *salt, size_t salt_len, int iterations, unsigned char *out, size_t out_len);

// Generate random bytes into buf
void crypto_random_bytes(unsigned char *buf, size_t len);

// encode binary to hex (lowercase). out must have at least bin_len*2+1 bytes.
void bin_to_hex(const unsigned char *bin, size_t bin_len, char *out);
// decode hex string into binary. out must have at least hex_len/2 bytes. Returns number of bytes written or -1 on error.
int hex_to_bin(const char *hex, unsigned char *out, size_t out_len);

#endif
