#ifndef SECURITY_AUDIT_H
#define SECURITY_AUDIT_H

#include <switch.h>
#include "crypto.h"

// Audit severity levels
typedef enum {
    AUDIT_INFO,
    AUDIT_WARNING,
    AUDIT_ERROR,
    AUDIT_CRITICAL
} AuditSeverity;

// Audit categories
typedef enum {
    AUDIT_CAT_FILESYSTEM,    // File permissions, integrity
    AUDIT_CAT_CRYPTO,        // Cryptographic operations
    AUDIT_CAT_MEMORY,        // Memory safety
    AUDIT_CAT_NETWORK,       // Network operations
    AUDIT_CAT_CONFIG,        // Configuration security
    AUDIT_CAT_ACCESS         // Access controls
} AuditCategory;

// Audit finding structure
typedef struct {
    AuditSeverity severity;
    AuditCategory category;
    char description[256];
    char recommendation[512];
    char location[256];      // File/component where issue was found
} AuditFinding;

// Audit report structure
typedef struct {
    AuditFinding *findings;
    size_t finding_count;
    size_t capacity;
    time_t timestamp;
    char version[32];
} AuditReport;

// Initialize security audit system
Result audit_init(void);
void audit_exit(void);

// Core audit functions
Result audit_run_full_scan(AuditReport *report);
Result audit_run_quick_scan(AuditReport *report);
Result audit_run_targeted_scan(AuditReport *report, AuditCategory category);

// Individual component audits
Result audit_check_filesystem(AuditReport *report);
Result audit_check_crypto(AuditReport *report);
Result audit_check_memory(AuditReport *report);
Result audit_check_network(AuditReport *report);
Result audit_check_config(AuditReport *report);
Result audit_check_access(AuditReport *report);

// Real-time monitoring
Result audit_start_monitoring(void (*callback)(const AuditFinding *finding));
void audit_stop_monitoring(void);

// Report management
Result audit_save_report(const AuditReport *report, const char *path);
Result audit_load_report(AuditReport *report, const char *path);
void audit_free_report(AuditReport *report);

// Utility functions
const char* audit_severity_string(AuditSeverity severity);
const char* audit_category_string(AuditCategory category);
Result audit_export_html(const AuditReport *report, const char *path);
Result audit_export_json(const AuditReport *report, const char *path);

#endif // SECURITY_AUDIT_H