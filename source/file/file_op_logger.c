#include "file_op_logger.h"
#include "../logger.h"
#include "../security/security_mode.h"
#include <stdio.h>
#include <string.h>

// Security check helper
static bool check_operation_security(FileOpType op, const char* src, const char* dst) {
    AccessFlags required_access = ACCESS_READ;
    
    switch (op) {
        case FILE_OP_COPY:
            required_access |= ACCESS_WRITE;
            break;
        case FILE_OP_MOVE:
        case FILE_OP_RENAME:
            required_access |= ACCESS_WRITE | ACCESS_DELETE;
            break;
        case FILE_OP_DELETE:
            required_access |= ACCESS_DELETE;
            break;
        case FILE_OP_CREATE:
        case FILE_OP_MKDIR:
            required_access |= ACCESS_WRITE;
            break;
    }
    
    // Check source access if applicable
    if (src && !security_operation_allowed(src, required_access)) {
        return false;
    }
    
    // Check destination access if applicable
    if (dst && !security_operation_allowed(dst, ACCESS_WRITE)) {
        return false;
    }
    
    return true;
}

// Operation type to string conversion
const char* file_op_type_str(FileOpType op) {
    switch (op) {
        case FILE_OP_COPY: return "COPY";
        case FILE_OP_MOVE: return "MOVE";
        case FILE_OP_DELETE: return "DELETE";
        case FILE_OP_RENAME: return "RENAME";
        case FILE_OP_CREATE: return "CREATE";
        case FILE_OP_MKDIR: return "MKDIR";
        default: return "UNKNOWN";
    }
}

// Check if operation can be undone
bool file_op_is_undoable(FileOpType op) {
    switch (op) {
        case FILE_OP_MOVE:
        case FILE_OP_RENAME:
        case FILE_OP_DELETE:
            return true;
        default:
            return false;
    }
}

// Log operation start
void log_file_op_start(FileOpType op, const char* src, const char* dst) {
    char message[256], details[1024];
    
    // Check security before operation
    if (!check_operation_security(op, src, dst)) {
        snprintf(message, sizeof(message), "%s operation blocked by security", 
                file_op_type_str(op));
        snprintf(details, sizeof(details), 
                "Source: %s\nDestination: %s\nMode: %s",
                src ? src : "N/A",
                dst ? dst : "N/A",
                security_mode_to_string(security_get_mode()));
                
        log_event(LOG_SECURITY, message, details);
        return;
    }
    
    snprintf(message, sizeof(message), "Starting %s operation", file_op_type_str(op));
    snprintf(details, sizeof(details), "Source: %s\nDestination: %s",
             src ? src : "N/A",
             dst ? dst : "N/A");
             
    log_event(LOG_FILE_OP, message, details);
}

// Log operation completion
void log_file_op_complete(FileOpType op, const char* src, const char* dst, bool success) {
    char message[256], details[1024];
    
    snprintf(message, sizeof(message), "%s operation %s",
             file_op_type_str(op),
             success ? "completed" : "failed");
             
    snprintf(details, sizeof(details),
             "Source: %s\nDestination: %s\nUndoable: %s",
             src ? src : "N/A",
             dst ? dst : "N/A",
             file_op_is_undoable(op) ? "Yes" : "No");
             
    log_event(LOG_FILE_OP, message, details);
}

// Log operation error
void log_file_op_error(FileOpType op, const char* src, const char* dst, const char* error) {
    char message[256], details[1024];
    
    snprintf(message, sizeof(message), "%s operation failed", file_op_type_str(op));
    snprintf(details, sizeof(details),
             "Source: %s\nDestination: %s\nError: %s",
             src ? src : "N/A",
             dst ? dst : "N/A",
             error ? error : "Unknown error");
             
    log_event(LOG_ERROR, message, details);
}