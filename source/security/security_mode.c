#include "security_mode.h"
#include "../settings.h"
#include "../logger.h"
#include "../ui.h"
#include "../fs.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// Critical system paths that need protection
static const ProtectedPath DEFAULT_PROTECTED_PATHS[] = {
    {"sdmc:/switch/DBFM/system", ACCESS_READ, true},
    {"sdmc:/switch/DBFM/plugins", ACCESS_READ | ACCESS_EXECUTE, true},
    {"sdmc:/nintendo", ACCESS_READ, true},
    {"sdmc:/switch/DBFM/settings.cfg", ACCESS_READ, false}
};

static SecurityContext g_security_ctx = {
    .current_mode = SECURITY_MODE_NORMAL,
    .parental_pin = 0,
    .pin_verified = false,
    .guest_session_start = 0,
    .guest_timeout_mins = 30,
    .current_access = ACCESS_FULL
};

// Protected paths list
static ProtectedPath* g_protected_paths = NULL;
static size_t g_protected_paths_count = 0;

// Initialize security system
Result security_init(void) {
    // Load default protected paths
    for (size_t i = 0; i < sizeof(DEFAULT_PROTECTED_PATHS) / sizeof(ProtectedPath); i++) {
        security_add_protected_path(
            DEFAULT_PROTECTED_PATHS[i].path,
            DEFAULT_PROTECTED_PATHS[i].allowed_access,
            DEFAULT_PROTECTED_PATHS[i].recursive
        );
    }
    
    log_event(LOG_SECURITY, "Security system initialized", 
              "Default protection rules applied");
    return 0;
}

void security_exit(void) {
    if (g_protected_paths) {
        free(g_protected_paths);
        g_protected_paths = NULL;
    }
    g_protected_paths_count = 0;
}

// Mode management
Result security_set_mode(SecurityMode mode) {
    SecurityMode old_mode = g_security_ctx.current_mode;
    g_security_ctx.current_mode = mode;
    
    // Update access flags based on mode
    switch (mode) {
        case SECURITY_MODE_NORMAL:
            g_security_ctx.current_access = ACCESS_FULL;
            break;
        case SECURITY_MODE_GUEST:
            g_security_ctx.current_access = ACCESS_READ | ACCESS_EXECUTE;
            g_security_ctx.guest_session_start = time(NULL);
            break;
        case SECURITY_MODE_SAFE:
            g_security_ctx.current_access = ACCESS_READ | ACCESS_EXECUTE;
            break;
        case SECURITY_MODE_PARENTAL:
            if (!g_security_ctx.pin_verified) {
                g_security_ctx.current_access = ACCESS_READ;
            }
            break;
    }
    
    log_event(LOG_SECURITY, "Security mode changed",
              "From %s to %s", 
              security_mode_to_string(old_mode),
              security_mode_to_string(mode));
    
    return 0;
}

SecurityMode security_get_mode(void) {
    return g_security_ctx.current_mode;
}

const char* security_mode_to_string(SecurityMode mode) {
    switch (mode) {
        case SECURITY_MODE_NORMAL: return "Normal";
        case SECURITY_MODE_GUEST: return "Guest";
        case SECURITY_MODE_SAFE: return "Safe";
        case SECURITY_MODE_PARENTAL: return "Parental";
        default: return "Unknown";
    }
}

// PIN management
static u32 hash_pin(const char* pin) {
    u32 hash = 5381;
    int c;
    while ((c = *pin++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

Result security_set_pin(const char* pin) {
    if (!pin || strlen(pin) != 4) return -1;
    g_security_ctx.parental_pin = hash_pin(pin);
    g_security_ctx.pin_verified = false;
    
    log_event(LOG_SECURITY, "Parental PIN changed", "PIN updated successfully");
    return 0;
}

Result security_verify_pin(const char* pin) {
    if (!pin) return -1;
    
    bool verified = (hash_pin(pin) == g_security_ctx.parental_pin);
    g_security_ctx.pin_verified = verified;
    
    if (verified) {
        g_security_ctx.current_access = ACCESS_FULL;
        log_event(LOG_SECURITY, "PIN verified", "Full access granted");
    } else {
        log_event(LOG_SECURITY, "PIN verification failed", "Access restricted");
    }
    
    return verified ? 0 : -1;
}

// Guest session management
Result security_start_guest_session(u32 timeout_mins) {
    g_security_ctx.guest_timeout_mins = timeout_mins;
    g_security_ctx.guest_session_start = time(NULL);
    return security_set_mode(SECURITY_MODE_GUEST);
}

Result security_end_guest_session(void) {
    if (g_security_ctx.current_mode == SECURITY_MODE_GUEST) {
        return security_set_mode(SECURITY_MODE_NORMAL);
    }
    return 0;
}

bool security_is_guest_session_expired(void) {
    if (g_security_ctx.current_mode != SECURITY_MODE_GUEST) return false;
    
    time_t now = time(NULL);
    double elapsed_mins = difftime(now, g_security_ctx.guest_session_start) / 60.0;
    
    if (elapsed_mins >= g_security_ctx.guest_timeout_mins) {
        security_end_guest_session();
        log_event(LOG_SECURITY, "Guest session expired", 
                 "Session duration: %.1f minutes", elapsed_mins);
        return true;
    }
    
    return false;
}

// Access control
bool security_check_access(const char* path, AccessFlags requested_access) {
    if (!path) return false;
    
    // Always allow read access to UI and essential files
    if (requested_access == ACCESS_READ) {
        if (strstr(path, "romfs:/") == path) return true;
    }
    
    // Check if path is protected
    for (size_t i = 0; i < g_protected_paths_count; i++) {
        const ProtectedPath* pp = &g_protected_paths[i];
        
        // Check if path matches (directly or is under protected path if recursive)
        if (strcmp(path, pp->path) == 0 ||
            (pp->recursive && strstr(path, pp->path) == path)) {
            
            // Check if requested access is allowed
            if ((requested_access & pp->allowed_access) != requested_access) {
                log_event(LOG_SECURITY, "Access denied",
                         "Path: %s\nRequested: 0x%x, Allowed: 0x%x",
                         path, requested_access, pp->allowed_access);
                return false;
            }
        }
    }
    
    // Check current mode restrictions
    if ((requested_access & g_security_ctx.current_access) != requested_access) {
        log_event(LOG_SECURITY, "Access denied by mode",
                 "Path: %s\nMode: %s", 
                 path, security_mode_to_string(g_security_ctx.current_mode));
        return false;
    }
    
    return true;
}

Result security_add_protected_path(const char* path, AccessFlags allowed_access, bool recursive) {
    ProtectedPath* new_paths = realloc(g_protected_paths, 
                                     (g_protected_paths_count + 1) * sizeof(ProtectedPath));
    if (!new_paths) return -1;
    
    g_protected_paths = new_paths;
    ProtectedPath* pp = &g_protected_paths[g_protected_paths_count++];
    
    pp->path = strdup(path);
    pp->allowed_access = allowed_access;
    pp->recursive = recursive;
    
    log_event(LOG_SECURITY, "Protected path added",
             "Path: %s\nAccess: 0x%x\nRecursive: %s",
             path, allowed_access, recursive ? "Yes" : "No");
    
    return 0;
}

// Safe mode checks
bool security_is_critical_file(const char* path) {
    const char* critical_patterns[] = {
        "settings.cfg",
        "/system/",
        "/nintendo/",
        "nsp_manifest.json",
        NULL
    };
    
    for (const char** pattern = critical_patterns; *pattern; pattern++) {
        if (strstr(path, *pattern)) return true;
    }
    
    return false;
}

bool security_operation_allowed(const char* path, AccessFlags operation) {
    // In safe mode, prevent modification of critical files
    if (g_security_ctx.current_mode == SECURITY_MODE_SAFE &&
        security_is_critical_file(path) &&
        (operation & (ACCESS_WRITE | ACCESS_DELETE))) {
        return false;
    }
    
    return security_check_access(path, operation);
}

// Settings UI
void security_show_settings(void) {
    bool exit_requested = false;
    char pin_buffer[5] = {0};
    
    while (!exit_requested) {
        ui_begin_frame();
        
        ui_header("Security Settings");
        
        // Mode selection
        ui_header_sub("Security Mode");
        if (ui_button("Normal Mode")) {
            security_set_mode(SECURITY_MODE_NORMAL);
        }
        if (ui_button("Guest Mode")) {
            u32 mins = 30; // Default timeout
            security_start_guest_session(mins);
        }
        if (ui_button("Safe Mode")) {
            security_set_mode(SECURITY_MODE_SAFE);
        }
        if (ui_button("Parental Controls")) {
            security_set_mode(SECURITY_MODE_PARENTAL);
        }
        
        // Current status
        ui_header_sub("Current Status");
        ui_label("Mode: %s", security_mode_to_string(g_security_ctx.current_mode));
        ui_label("Access Level: 0x%x", g_security_ctx.current_access);
        
        if (g_security_ctx.current_mode == SECURITY_MODE_GUEST) {
            time_t now = time(NULL);
            double mins_left = g_security_ctx.guest_timeout_mins - 
                             difftime(now, g_security_ctx.guest_session_start) / 60.0;
            if (mins_left > 0) {
                ui_label("Guest session: %.1f minutes remaining", mins_left);
            } else {
                ui_label_warning("Guest session expired");
            }
        }
        
        // PIN management
        if (g_security_ctx.current_mode == SECURITY_MODE_PARENTAL) {
            ui_header_sub("PIN Management");
            
            if (!g_security_ctx.pin_verified) {
                ui_label("Enter PIN to unlock:");
                // Note: In a real implementation, use proper secure input
                if (ui_button("Verify PIN")) {
                    security_verify_pin(pin_buffer);
                }
            } else {
                if (ui_button("Change PIN")) {
                    // Show PIN change dialog
                }
                if (ui_button("Lock")) {
                    g_security_ctx.pin_verified = false;
                }
            }
        }
        
        if (ui_button("Back")) {
            exit_requested = true;
        }
        
        ui_end_frame();
    }
}