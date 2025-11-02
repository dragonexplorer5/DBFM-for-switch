#include "secure.h"
#include "crypto.h"
#include "fs.h"
#include "ui.h"
#include <string.h>
#include <malloc.h>
#include <time.h>

#define SECURE_LOG_FILE "sdmc:/switch/dbfm/secure.log"
#define SECURE_WIPE_PASSES 3
#define SECURE_PATH_MAX 0x300

// Restricted paths that require high security
static const char* RESTRICTED_PATHS[] = {
    "save:/",
    "bis:/",
    "system:/",
    "safe:/",
    "/atmosphere",
    "/sept",
    "/bootloader",
    NULL
};

// Allowed file extensions for title installation
static const char* ALLOWED_INSTALL_EXTS[] = {
    ".nsp",
    ".nsz",
    ".xci",
    ".xcz",
    NULL
};

static bool s_initialized = false;
static char s_log_buffer[4096] = {0};
static size_t s_log_pos = 0;

Result secure_init(void) {
    if (s_initialized) return 0;

    // Create log directory
    fs_create_directories("sdmc:/switch/dbfm");
    
    // Initialize crypto for secure operations
    Result rc = crypto_init();
    if (R_SUCCEEDED(rc)) {
        s_initialized = true;
    }
    return rc;
}

void secure_exit(void) {
    if (!s_initialized) return;

    // Flush remaining log
    if (s_log_pos > 0) {
        FILE* f = fopen(SECURE_LOG_FILE, "a");
        if (f) {
            fwrite(s_log_buffer, 1, s_log_pos, f);
            fclose(f);
        }
    }

    crypto_exit();
    s_initialized = false;
}

static bool _is_path_restricted(const char* path) {
    for (const char** restricted = RESTRICTED_PATHS; *restricted; restricted++) {
        if (strstr(path, *restricted) == path) {
            return true;
        }
    }
    return false;
}

static bool _is_extension_allowed(const char* path) {
    const char* ext = strrchr(path, '.');
    if (!ext) return false;

    for (const char** allowed = ALLOWED_INSTALL_EXTS; *allowed; allowed++) {
        if (strcasecmp(ext, *allowed) == 0) {
            return true;
        }
    }
    return false;
}

Result secure_validate_operation(const SecureContext* ctx) {
    if (!s_initialized || !ctx) {
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    }

    // Path validation for file operations
    if (ctx->target_path) {
        if (!secure_validate_path(ctx->target_path)) {
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        }

        // Check if operation requires elevated security
        if (_is_path_restricted(ctx->target_path)) {
            if (ctx->level < SecureLevel_High) {
                return MAKERESULT(Module_Libnx, LibnxError_NotAllowed);
            }
        }
    }

    // Title ID validation
    if (ctx->title_id != 0) {
        Result rc = secure_validate_title_access(ctx->title_id);
        if (R_FAILED(rc)) return rc;
    }

    // User confirmation for sensitive operations
    if (ctx->requires_confirmation) {
        char message[256];
        snprintf(message, sizeof(message),
                "Are you sure you want to perform this operation?\n\n"
                "Operation: %s\n"
                "Target: %s",
                ctx->operation_name,
                ctx->target_path ? ctx->target_path : "N/A");

        if (!ui_show_dialog("Security Confirmation", message)) {
            return MAKERESULT(Module_Libnx, LibnxError_RequestCanceled);
        }
    }

    return 0;
}

void* secure_alloc(size_t size) {
    void* ptr = aligned_alloc(0x1000, size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void secure_free(void* ptr) {
    if (!ptr) return;
    
    // Get allocation size (platform specific)
    size_t size = 0; // TODO: Implement for Switch
    
    // Wipe memory before freeing
    secure_wipe(ptr, size);
    free(ptr);
}

void secure_wipe(void* ptr, size_t size) {
    if (!ptr || size == 0) return;

    for (int pass = 0; pass < SECURE_WIPE_PASSES; pass++) {
        // Pass 1: 0x00
        // Pass 2: 0xFF
        // Pass 3: Random
        u8 pattern = (pass == 0) ? 0x00 : (pass == 1) ? 0xFF : 0x00;
        
        if (pass == 2) {
            // Random pass
            u8* data = (u8*)ptr;
            for (size_t i = 0; i < size; i++) {
                data[i] = (u8)rand();
            }
        } else {
            memset(ptr, pattern, size);
        }
    }
}

Result secure_remove_file(const char* path) {
    if (!s_initialized || !path) {
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    }

    // First securely wipe the file
    Result rc = secure_wipe_file(path);
    if (R_FAILED(rc)) return rc;

    // Then remove it
    if (remove(path) != 0) {
        return MAKERESULT(Module_Libnx, LibnxError_IoError);
    }

    return 0;
}

Result secure_wipe_file(const char* path) {
    if (!s_initialized || !path) {
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    }

    FILE* f = fopen(path, "r+b");
    if (!f) return MAKERESULT(Module_Libnx, LibnxError_NotFound);

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    u8* buffer = secure_alloc(0x4000); // 16KB buffer
    if (!buffer) {
        fclose(f);
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }

    Result rc = 0;
    for (int pass = 0; pass < SECURE_WIPE_PASSES; pass++) {
        u8 pattern = (pass == 0) ? 0x00 : (pass == 1) ? 0xFF : 0x00;
        
        if (pass == 2) {
            // Random pass
            for (size_t i = 0; i < 0x4000; i++) {
                buffer[i] = (u8)rand();
            }
        } else {
            memset(buffer, pattern, 0x4000);
        }

        size_t remaining = size;
        fseek(f, 0, SEEK_SET);

        while (remaining > 0) {
            size_t to_write = (remaining > 0x4000) ? 0x4000 : remaining;
            if (fwrite(buffer, 1, to_write, f) != to_write) {
                rc = MAKERESULT(Module_Libnx, LibnxError_IoError);
                break;
            }
            remaining -= to_write;
        }

        if (R_FAILED(rc)) break;
        fflush(f);
    }

    secure_free(buffer);
    fclose(f);
    return rc;
}

Result secure_move_file(const char* src, const char* dst) {
    if (!s_initialized || !src || !dst) {
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    }

    // Create secure copy
    FILE* in = fopen(src, "rb");
    if (!in) return MAKERESULT(Module_Libnx, LibnxError_NotFound);

    FILE* out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return MAKERESULT(Module_Libnx, LibnxError_IoError);
    }

    u8* buffer = secure_alloc(0x4000);
    if (!buffer) {
        fclose(in);
        fclose(out);
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }

    Result rc = 0;
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, 0x4000, in)) > 0) {
        if (fwrite(buffer, 1, bytes_read, out) != bytes_read) {
            rc = MAKERESULT(Module_Libnx, LibnxError_IoError);
            break;
        }
    }

    secure_free(buffer);
    fclose(in);
    fclose(out);

    if (R_SUCCEEDED(rc)) {
        // Verify the copy
        FILE* src_verify = fopen(src, "rb");
        FILE* dst_verify = fopen(dst, "rb");
        
        if (src_verify && dst_verify) {
            u8 src_hash[32], dst_hash[32];
            // TODO: Calculate SHA-256 of both files
            if (memcmp(src_hash, dst_hash, 32) != 0) {
                rc = MAKERESULT(Module_Libnx, LibnxError_VerificationFailed);
            }
        }
        
        if (src_verify) fclose(src_verify);
        if (dst_verify) fclose(dst_verify);
    }

    if (R_SUCCEEDED(rc)) {
        // Only remove source after successful copy and verification
        rc = secure_remove_file(src);
    } else {
        // Clean up failed destination
        remove(dst);
    }

    return rc;
}

bool secure_validate_path(const char* path) {
    if (!path) return false;
    
    // Check path length
    if (strlen(path) >= SECURE_PATH_MAX) return false;
    
    // Check for path traversal attempts
    if (strstr(path, "..")) return false;
    
    // Check for invalid characters
    const char* invalid_chars = "<>:\"|?*";
    for (const char* c = invalid_chars; *c; c++) {
        if (strchr(path, *c)) return false;
    }
    
    return true;
}

bool secure_is_path_allowed(const char* path) {
    if (!secure_validate_path(path)) return false;
    return !_is_path_restricted(path);
}

Result secure_validate_title_access(u64 title_id) {
    if (!s_initialized) {
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    }

    // Check if title ID is valid
    if (title_id == 0) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    // TODO: Add more title ID validation logic
    return 0;
}

Result secure_validate_title_install(const char* nsp_path) {
    if (!s_initialized || !nsp_path) {
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    }

    // Check file extension
    if (!_is_extension_allowed(nsp_path)) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    // Verify NSP integrity
    NspVerifyResult verify_result;
    Result rc = verify_nsp_file(nsp_path, &verify_result);
    if (R_FAILED(rc)) return rc;

    // Check NSP contents
    if (!verify_result.valid_format ||
        !verify_result.has_program ||
        !verify_result.has_meta) {
        verify_free_nsp_result(&verify_result);
        return MAKERESULT(Module_Libnx, LibnxError_VerificationFailed);
    }

    // Check if we have required ticket
    if (verify_result.requires_ticket && !verify_result.has_ticket) {
        verify_free_nsp_result(&verify_result);
        return MAKERESULT(Module_Libnx, LibnxError_MissingTicket);
    }

    verify_free_nsp_result(&verify_result);
    return 0;
}

Result secure_validate_key_import(const void* key_data, size_t key_size) {
    if (!s_initialized || !key_data || key_size != 16) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    // Validate key data
    const u8* key = (const u8*)key_data;
    bool all_zero = true;
    for (size_t i = 0; i < key_size; i++) {
        if (key[i] != 0) {
            all_zero = false;
            break;
        }
    }

    if (all_zero) {
        return MAKERESULT(Module_Libnx, LibnxError_InvalidKey);
    }

    return 0;
}

Result secure_validate_ticket_import(const void* ticket_data, size_t ticket_size) {
    if (!s_initialized || !ticket_data || ticket_size < 0x400) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    // Check ticket magic
    if (memcmp(ticket_data, "Root-CA00000003-XS00000020", 27) != 0) {
        return MAKERESULT(Module_Libnx, LibnxError_BadMagic);
    }

    // TODO: Add signature verification

    return 0;
}

Result secure_log_operation(const SecureContext* ctx, Result operation_result) {
    if (!s_initialized || !ctx) {
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    }

    time_t now;
    time(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    char log_entry[512];
    snprintf(log_entry, sizeof(log_entry),
             "[%s] Op: %s, Level: %d, Path: %s, TID: %016lx, Result: 0x%x\n",
             timestamp,
             ctx->operation_name,
             ctx->level,
             ctx->target_path ? ctx->target_path : "N/A",
             ctx->title_id,
             operation_result);

    size_t entry_len = strlen(log_entry);

    // If buffer would overflow, flush it first
    if (s_log_pos + entry_len >= sizeof(s_log_buffer)) {
        FILE* f = fopen(SECURE_LOG_FILE, "a");
        if (f) {
            fwrite(s_log_buffer, 1, s_log_pos, f);
            fclose(f);
        }
        s_log_pos = 0;
    }

    // Add to buffer
    memcpy(s_log_buffer + s_log_pos, log_entry, entry_len);
    s_log_pos += entry_len;

    return 0;
}

void secure_get_operation_log(char* out_log, size_t max_size) {
    if (!out_log || max_size == 0) return;

    FILE* f = fopen(SECURE_LOG_FILE, "r");
    if (!f) {
        strncpy(out_log, "No operation log available", max_size - 1);
        out_log[max_size - 1] = '\0';
        return;
    }

    size_t bytes_read = fread(out_log, 1, max_size - 1, f);
    out_log[bytes_read] = '\0';
    fclose(f);
}

const char* secure_get_error_message(Result rc) {
    if (R_SUCCEEDED(rc)) return "Success";

    switch (rc) {
        case MAKERESULT(Module_Libnx, LibnxError_NotInitialized):
            return "Security system not initialized";
        case MAKERESULT(Module_Libnx, LibnxError_BadInput):
            return "Invalid input parameters";
        case MAKERESULT(Module_Libnx, LibnxError_NotAllowed):
            return "Operation not allowed (insufficient privileges)";
        case MAKERESULT(Module_Libnx, LibnxError_NotFound):
            return "File or resource not found";
        case MAKERESULT(Module_Libnx, LibnxError_IoError):
            return "I/O error during operation";
        case MAKERESULT(Module_Libnx, LibnxError_OutOfMemory):
            return "Out of memory";
        case MAKERESULT(Module_Libnx, LibnxError_BadMagic):
            return "Invalid file magic/signature";
        case MAKERESULT(Module_Libnx, LibnxError_InvalidKey):
            return "Invalid key data";
        case MAKERESULT(Module_Libnx, LibnxError_VerificationFailed):
            return "Verification failed";
        case MAKERESULT(Module_Libnx, LibnxError_MissingTicket):
            return "Required ticket is missing";
        case MAKERESULT(Module_Libnx, LibnxError_RequestCanceled):
            return "Operation cancelled by user";
        default:
            return "Unknown error";
    }
}