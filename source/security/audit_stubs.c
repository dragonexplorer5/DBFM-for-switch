#include <switch.h>
#include "security_audit.h"

Result audit_init(void) {
    return 0; // Stub success
}

Result audit_run_quick_scan(AuditReport* report) {
    return 0; // Stub success
}

Result audit_save_report(const AuditReport* report, const char* path) {
    return 0; // Stub success
}

void audit_free_report(AuditReport* report) {
    // No-op stub
}

void audit_exit(void) {
    // No-op stub
}