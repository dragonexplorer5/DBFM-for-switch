#ifndef SECURITY_MODE_H
#define SECURITY_MODE_H

#include <switch.h>
#include <stdbool.h>

// Security modes
typedef enum {
    SECURITY_MODE_NORMAL = 0,   // Full access
    SECURITY_MODE_GUEST,        // Limited access, temporary session
    SECURITY_MODE_SAFE,         // Prevent critical modifications
    SECURITY_MODE_PARENTAL      // PIN-protected access
} SecurityMode;

// Access flags
typedef enum {
    ACCESS_NONE = 0,
    ACCESS_READ = (1 << 0),
    ACCESS_WRITE = (1 << 1),
    ACCESS_DELETE = (1 << 2),
    ACCESS_EXECUTE = (1 << 3),
    ACCESS_SYSTEM = (1 << 4),    // Access to system folders
    ACCESS_PLUGINS = (1 << 5),   // Access to plugins/extensions
    ACCESS_FULL = 0xFF
} AccessFlags;

// Protected paths configuration
typedef struct {
    const char* path;           // Path to protect
    AccessFlags allowed_access; // Allowed operations
    bool recursive;            // Apply to subdirectories
} ProtectedPath;

// Security context
typedef struct {
    SecurityMode current_mode;
    u32 parental_pin;           // Hashed PIN
    bool pin_verified;          // PIN verification status
    time_t guest_session_start; // For guest mode timeout
    u32 guest_timeout_mins;     // Guest session duration
    AccessFlags current_access; // Current access level
} SecurityContext;

// Function prototypes
Result security_init(void);
void security_exit(void);

// Mode management
Result security_set_mode(SecurityMode mode);
SecurityMode security_get_mode(void);
const char* security_mode_to_string(SecurityMode mode);

// Parental controls
Result security_set_pin(const char* pin);
Result security_verify_pin(const char* pin);
Result security_reset_pin(const char* current_pin);
bool security_is_pin_verified(void);

// Guest mode
Result security_start_guest_session(u32 timeout_mins);
Result security_end_guest_session(void);
bool security_is_guest_session_expired(void);

// Access control
bool security_check_access(const char* path, AccessFlags requested_access);
Result security_add_protected_path(const char* path, AccessFlags allowed_access, bool recursive);
Result security_remove_protected_path(const char* path);

// Safe mode
bool security_is_critical_file(const char* path);
bool security_operation_allowed(const char* path, AccessFlags operation);

// UI
void security_show_settings(void);

#endif // SECURITY_MODE_H