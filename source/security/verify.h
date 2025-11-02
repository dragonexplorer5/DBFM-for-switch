#ifndef VERIFY_H
#define VERIFY_H

#include <switch.h>

// NCA content types
typedef enum {
    NcaType_Program,
    NcaType_Meta,
    NcaType_Control,
    NcaType_Manual,
    NcaType_Data,
    NcaType_PublicData
} NcaType;

// NCA verification result
typedef struct {
    bool valid_header;
    bool valid_signature;
    NcaType type;
    u64 title_id;
    u32 content_type;
    u8 crypto_type;
    u8 key_gen;
    u8 rights_id[16];
    bool has_rights_id;
    bool is_ticket_missing;
} NcaVerifyResult;

// NSP verification result
typedef struct {
    bool valid_format;
    bool has_program;
    bool has_control;
    bool has_legal;
    bool has_meta;
    size_t nca_count;
    char title_name[0x201];
    u64 title_id;
    u8 min_key_gen;
    bool requires_ticket;
    bool has_ticket;
    NcaVerifyResult* nca_results;
} NspVerifyResult;

// Initialize verification system
Result verify_init(void);
void verify_exit(void);

// NCA verification
Result verify_nca_file(const char* path, NcaVerifyResult* out_result);
Result verify_nca_memory(const void* data, size_t size, NcaVerifyResult* out_result);

// NSP verification
Result verify_nsp_file(const char* path, NspVerifyResult* out_result);
void verify_free_nsp_result(NspVerifyResult* result);

// Error handling
const char* verify_get_error_message(Result rc);
const char* verify_get_content_type_string(NcaType type);

#endif // VERIFY_H