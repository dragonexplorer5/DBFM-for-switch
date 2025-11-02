#ifndef APP_H
#define APP_H

#include "crypto.h"
#include "secure_validation.h"
#include "security_audit.h"

// Application security levels
typedef enum {
    APP_SECURITY_NORMAL = 0,     // Normal operation, no warnings
    APP_SECURITY_ELEVATED,       // Elevated privileges needed
    APP_SECURITY_WARNING,        // Security warning present
    APP_SECURITY_LOCKDOWN        // Security violation detected
} AppSecurityState;

// Application states
typedef enum {
    APP_STATE_FILE_BROWSER,
    APP_STATE_SAVE_MANAGER,
    APP_STATE_NSP_MANAGER,
    APP_STATE_SYSTEM_TOOLS,
    APP_STATE_HB_STORE,
    APP_STATE_SETTINGS,
    APP_STATE_TASK_QUEUE,
    APP_STATE_FIRMWARE_MANAGER,
    APP_STATE_GOLDLEAF_FEATURES,
    APP_STATE_SECURITY_AUDIT,    // New security audit state
    APP_STATE_SECURITY_SETTINGS  // New security settings state
} AppState;

// Application security settings
typedef struct {
    bool enable_audit_logging;           // Log security events
    bool require_confirmations;          // Require confirmation for risky ops
    bool encrypt_sensitive_data;         // Encrypt sensitive data
    bool enforce_validation;             // Enforce strict validation
    ValidationFlags validation_flags;     // Active validation flags
    AuditCategory audit_categories;      // Active audit categories
    unsigned int min_password_length;    // Minimum password length
    unsigned int max_failed_attempts;    // Max failed login attempts
} AppSecuritySettings;

// Initialize application
Result app_init(void);

// Clean up application
void app_exit(void);

// Main application loop
void app_run(void);

// Security-related functions
Result app_init_security(void);
void app_exit_security(void);
Result app_update_security_state(void);
AppSecurityState app_get_security_state(void);
Result app_set_security_settings(const AppSecuritySettings *settings);
Result app_get_security_settings(AppSecuritySettings *settings);
Result app_run_security_audit(void);
Result app_handle_security_event(const AuditFinding *finding);

// Secure operations
Result app_secure_operation(const char *operation, bool require_confirm);
Result app_validate_file(const char *path, ValidationFlags flags);
Result app_encrypt_sensitive_data(const void *data, size_t size);
Result app_decrypt_sensitive_data(void *out, size_t size);

// Enhanced state management
void app_set_state(AppState new_state);
AppState app_get_state(void);
bool app_state_requires_elevation(AppState state);

// Core application functions
void app_process_input(void);
void app_update(void);
void app_render(void);

// Security UI integration
void app_render_security_status(void);
void app_show_security_warning(const char *message);
Result app_prompt_for_confirmation(const char *operation);

#endif // APP_H