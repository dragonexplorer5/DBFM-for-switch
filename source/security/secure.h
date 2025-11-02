#ifndef SECURE_H
#define SECURE_H

#include <switch.h>

// Security levels for operations
typedef enum {
    SecureLevel_None,      // No security needed (reading public info)
    SecureLevel_Low,       // Basic security (file operations)
    SecureLevel_Medium,    // Enhanced security (system files)
    SecureLevel_High,      // Critical security (keys, tickets)
    SecureLevel_Critical   // Maximum security (crypto operations)
} SecureLevel;

// Operation validation context
typedef struct {
    SecureLevel level;
    bool requires_confirmation;
    bool requires_admin;
    const char* operation_name;
    const char* target_path;
    u64 title_id;
} SecureContext;

// Initialize security system
Result secure_init(void);
void secure_exit(void);

// Operation validation
Result secure_validate_operation(const SecureContext* ctx);

// Secure memory operations
void* secure_alloc(size_t size);
void secure_free(void* ptr);
void secure_wipe(void* ptr, size_t size);

// Secure file operations
Result secure_remove_file(const char* path);
Result secure_wipe_file(const char* path);
Result secure_move_file(const char* src, const char* dst);

// Path validation
bool secure_validate_path(const char* path);
bool secure_is_path_allowed(const char* path);

// Title operations validation
Result secure_validate_title_access(u64 title_id);
Result secure_validate_title_install(const char* nsp_path);

// Key validation
Result secure_validate_key_import(const void* key_data, size_t key_size);
Result secure_validate_ticket_import(const void* ticket_data, size_t ticket_size);

// Logging and monitoring
Result secure_log_operation(const SecureContext* ctx, Result operation_result);
void secure_get_operation_log(char* out_log, size_t max_size);

// Error handling
const char* secure_get_error_message(Result rc);

#endif // SECURE_H