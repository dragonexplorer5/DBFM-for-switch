#ifndef SECURE_VALIDATION_H
#define SECURE_VALIDATION_H

#include <switch.h>
#include "crypto.h"

// Validation flags
typedef enum {
    VALIDATE_SIGNATURE = 1 << 0,
    VALIDATE_HASH     = 1 << 1,
    VALIDATE_SIZE     = 1 << 2,
    VALIDATE_CONTENT  = 1 << 3,
    VALIDATE_PERMS    = 1 << 4,
    VALIDATE_ALL      = 0xFFFFFFFF
} ValidationFlags;

// Content validation rules
typedef struct {
    const char *pattern;     // Pattern to match/validate
    size_t max_length;      // Maximum allowed length
    bool allow_special;      // Allow special characters
    bool required;          // Field is required
} ContentRule;

// File validation context
typedef struct {
    ValidationFlags flags;
    unsigned char expected_hash[32];
    size_t min_size;
    size_t max_size;
    const char *allowed_extensions;
    ContentRule *content_rules;
    size_t rule_count;
} ValidationContext;

// Initialize validation system
Result validation_init(void);
void validation_exit(void);

// File validation
Result validation_check_file(const char *path, ValidationContext *ctx);
Result validation_check_nsp(const char *path, ValidationContext *ctx);
Result validation_check_nca(const char *path, ValidationContext *ctx);

// Install validation
Result validation_check_install_prerequisites(void);
Result validation_check_install_space(const char *path);
Result validation_check_install_permissions(void);
Result validation_verify_install_integrity(const char *path);

// Input validation
Result validation_check_input_string(const char *input, const ContentRule *rule);
Result validation_sanitize_input(const char *input, char *output, size_t out_size);
Result validation_check_path(const char *path, bool write_access);

// Real-time validation
Result validation_start_monitor(const char *path, 
                              void (*callback)(const char *path, Result result));
void validation_stop_monitor(void);

// Batch validation
Result validation_check_directory(const char *path, ValidationContext *ctx,
                                void (*progress)(size_t current, size_t total));

// Custom validation rules
Result validation_add_rule(ValidationContext *ctx, const ContentRule *rule);
void validation_clear_rules(ValidationContext *ctx);

// Utility functions
const char* validation_error_string(Result rc);
void validation_context_init(ValidationContext *ctx);
void validation_context_free(ValidationContext *ctx);

#endif // SECURE_VALIDATION_H