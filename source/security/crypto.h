#ifndef HELLO_CRYPTO_H
#define HELLO_CRYPTO_H

#include <stddef.h>
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
void bin_to_hex(const unsigned char *bin, size_t bin_len, char *out);
int hex_to_bin(const char *hex, unsigned char *out, size_t out_len);

// Error handling
const char* crypto_error_string(Result rc);

#endif
