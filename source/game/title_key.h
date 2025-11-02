#ifndef TITLE_KEY_H
#define TITLE_KEY_H

#include <switch.h>

// Title key information
typedef struct {
    u64 title_id;
    u8 key[16];
    u8 rights_id[16];
    bool in_use;
} TitleKeyInfo;

// Initialize title key management
Result titlekey_init(void);
void titlekey_exit(void);

// Title key operations
Result titlekey_import(const void* ticket_data, size_t ticket_size);
Result titlekey_export(u64 title_id, void* out_key);
Result titlekey_remove(u64 title_id);

// Key database operations
Result titlekey_list(TitleKeyInfo** out_keys, size_t* out_count);
void titlekey_free_list(TitleKeyInfo* keys);

// Search and verify
Result titlekey_exists(u64 title_id, bool* out_exists);
Result titlekey_verify(const void* key_data, size_t key_size,
                      const u8* rights_id);

// Error handling
const char* titlekey_get_error(Result rc);

#endif // TITLE_KEY_H