#ifndef HELLO_CRYPTO_H
#define HELLO_CRYPTO_H

#include <stddef.h>
#include <stdint.h>
#include <switch.h>

// Encryption modes for different security needs
typedef enum {
    CRYPTO_MODE_AES_XTS, // For large file encryption (NSP/NCA)
    CRYPTO_MODE_AES_GCM, // For authenticated encryption (logs, sensitive data)
    CRYPTO_MODE_AES_CTR  // For stream encryption
} CryptoMode;

// Key derivation context for secure key generation
typedef struct {
    unsigned char salt[32];
    size_t iterations;    // Recommended: 100000+
    size_t memory_cost;   // For memory-hard KDF (Argon2)
    size_t parallelism;   // For parallel processing
} KeyContext;

// Authenticated encryption context
typedef struct {
    unsigned char tag[16];    // Authentication tag
    size_t tag_len;
    unsigned char nonce[12];  // Unique nonce for GCM
    size_t data_len;
} AuthContext;

// Core cryptographic functions
int pbkdf2_hmac_sha256(const char *password, const unsigned char *salt, 
                       size_t salt_len, int iterations, 
                       unsigned char *out, size_t out_len);
Result crypto_encrypt(const void *data, size_t data_len,
                     void *out, size_t *out_len,
                     const unsigned char *key,
                     CryptoMode mode, AuthContext *auth);
Result crypto_decrypt(const void *data, size_t data_len,
                     void *out, size_t *out_len,
                     const unsigned char *key,
                     CryptoMode mode, AuthContext *auth);

// Secure random generation
void crypto_random_bytes(unsigned char *buf, size_t len);
Result crypto_generate_key(unsigned char *key, size_t key_len);
Result crypto_generate_salt(unsigned char *salt, size_t salt_len);

// Title key functions
Result crypto_decrypt_title_key(const void *enc_key, const void *rights_id, void *out_key);
// Returns whether a title key (ticket) is available for the given rights_id.
// out_has_key will be set to true if a title key is available. This is a
// simple compatibility shim used by the verify code.
Result crypto_has_title_key(const void *rights_id, bool *out_has_key);

// Encrypt a raw title key using a rights ID (compatibility shim).
// Some code expects crypto_encrypt_title_key to exist; provide a minimal
// implementation that copies the key (no real encryption) so the project
// can build and run. Replace with a proper implementation if required.
Result crypto_encrypt_title_key(const void *title_key, const void *rights_id, void *out_enc_key);

// File encryption functions
Result crypto_encrypt_file(const char *in_path, const char *out_path,
                         const unsigned char *key, CryptoMode mode,
                         void (*progress)(size_t current, size_t total));
Result crypto_decrypt_file(const char *in_path, const char *out_path,
                         const unsigned char *key, CryptoMode mode,
                         void (*progress)(size_t current, size_t total));

// Log encryption functions
Result crypto_encrypt_log(const char *log_data, size_t data_len,
                         char *out_data, size_t *out_len);
Result crypto_decrypt_log(const char *enc_data, size_t data_len,
                         char *out_data, size_t *out_len);

// Secure memory functions
Result crypto_secure_wipe(void *data, size_t len);
Result crypto_secure_compare(const void *a, const void *b, size_t len);

// Utility functions
// Convert binary to hex. 'out' must have space for bin_len*2 + 1 bytes.
// A safer variant taking the output buffer length is provided for call sites
// that pass the buffer size.
void bin_to_hex(const unsigned char *bin, size_t bin_len, char *out);
void bin_to_hex_s(const unsigned char *bin, size_t bin_len, char *out, size_t out_len);
int hex_to_bin(const char *hex, unsigned char *out, size_t out_len);

// SHA-256 helper (32-byte output)
void crypto_sha256(const void *data, size_t len, unsigned char out[32]);

// Initialization/teardown for crypto subsystem
Result crypto_init(void);
void crypto_exit(void);
// Error handling
const char* crypto_error_string(Result rc);

#endif
