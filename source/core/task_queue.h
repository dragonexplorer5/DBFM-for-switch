#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include <stdbool.h>
#include <limits.h>
#include "crypto.h"
#include "security_audit.h"
#include "ui.h"

typedef enum {
    // File operations
    TASK_COPY,
    TASK_MOVE,
    TASK_DELETE,
    
    // Save operations
    TASK_BACKUP_SAVE,
    TASK_RESTORE_SAVE,
    
    // NSP operations
    TASK_DUMP_NSP,
    TASK_INSTALL_NSP,
    
    // System operations
    TASK_DUMP_SYSTEM,
    TASK_RESTORE_SYSTEM,
    TASK_DOWNLOAD_HB,
    
    // Security operations
    TASK_ENCRYPT_FILE,          // Encrypt a file
    TASK_DECRYPT_FILE,          // Decrypt a file
    TASK_VALIDATE_FILE,         // Validate file integrity
    TASK_SECURITY_AUDIT,        // Run security audit
    TASK_KEY_ROTATION,          // Rotate encryption keys
    TASK_SECURE_WIPE,          // Secure data wiping
    TASK_VERIFY_INSTALL,        // Verify installation
    TASK_UPDATE_HASHES,         // Update hash database
    TASK_SCAN_THREATS          // Scan for security threats
} TaskType;

// Security task parameters
typedef struct {
    ValidationFlags validation_flags;     // Validation flags for file checks
    CryptoMode crypto_mode;              // Encryption mode
    bool secure_delete;                  // Use secure deletion
    bool verify_after;                   // Verify after operation
    const u8* key;                       // Encryption key if needed
    size_t key_size;                     // Key size
    AuthContext* auth_ctx;               // Authentication context
} SecurityTaskParams;

typedef struct {
    int progress;
    char error_msg[256];
    bool has_error;
    SecurityLevel security_level;         // Security level of operation
    AuditFinding* findings;              // Security findings if any
    size_t finding_count;                // Number of findings
} TaskStatus;

typedef struct Task {
    TaskType type;
    char src_path[PATH_MAX];
    char dst_path[PATH_MAX];
    TaskStatus status;
    SecurityTaskParams security;          // Security parameters
    bool requires_confirmation;           // Whether task needs confirmation
    bool is_privileged;                  // Whether task needs elevated privileges
    bool cancel;                          // Request cancellation from UI
    struct Task* next;
    void *op_ctx;                          // opaque per-task operation context
} Task;

// Aggregated operations across the queue
int task_queue_get_aggregate_progress(void); // 0..100
void task_queue_cancel_all(void); // request cancel for all tasks (current + pending)
void task_queue_cancel_pending(void); // request cancel for pending tasks only

// Queue management
void task_queue_init(void);
void task_queue_add(TaskType type, const char* src, const char* dst);
void task_queue_add_secure(TaskType type, const char* src, const char* dst,
                          const SecurityTaskParams* security_params);
bool task_queue_is_empty(void);
Task* task_queue_get_current(void);
void task_queue_process(void);
void task_queue_clear(void);

// Security-enhanced task management
Result task_queue_validate_operation(Task* task);
Result task_queue_check_privileges(Task* task);
Result task_queue_audit_operation(Task* task);
void task_queue_log_operation(Task* task);
Result task_queue_secure_cleanup(Task* task);

// Progress and status reporting
int task_get_progress(Task* task);
const char* task_get_error(Task* task);
bool task_has_error(Task* task);
SecurityLevel task_get_security_level(Task* task);
const AuditFinding* task_get_findings(Task* task, size_t* count);

// Task validation helpers
bool task_needs_confirmation(Task* task);
bool task_is_privileged(Task* task);
bool task_verify_integrity(Task* task);

#endif // TASK_QUEUE_H