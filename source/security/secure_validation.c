#include "secure_validation.h"
#include <string.h>

Result validation_init(void) {
    // Minimal compatibility shim: no-op initialization
    return 0;
}

void validation_exit(void) {
    // no-op
}

// Basic stubs for validation functions. These should be expanded with
// real validation logic if required at runtime.
Result validation_check_file(const char *path, ValidationContext *ctx) { (void)path; (void)ctx; return 0; }
Result validation_check_nsp(const char *path, ValidationContext *ctx) { (void)path; (void)ctx; return 0; }
Result validation_check_nca(const char *path, ValidationContext *ctx) { (void)path; (void)ctx; return 0; }
Result validation_check_install_prerequisites(void) { return 0; }
Result validation_check_install_space(const char *path) { (void)path; return 0; }
Result validation_check_install_permissions(void) { return 0; }
Result validation_verify_install_integrity(const char *path) { (void)path; return 0; }
Result validation_check_input_string(const char *input, const ContentRule *rule) { (void)input; (void)rule; return 0; }
Result validation_sanitize_input(const char *input, char *output, size_t out_size) { if (output && out_size) { strncpy(output, input, out_size-1); output[out_size-1]=0; } return 0; }
Result validation_check_path(const char *path, bool write_access) { (void)path; (void)write_access; return 0; }
Result validation_start_monitor(const char *path, void (*callback)(const char*,Result)) { (void)path; (void)callback; return 0; }
void validation_stop_monitor(void) {}
Result validation_check_directory(const char *path, ValidationContext *ctx, void (*progress)(size_t,size_t)) { (void)path; (void)ctx; (void)progress; return 0; }
Result validation_add_rule(ValidationContext *ctx, const ContentRule *rule) { (void)ctx; (void)rule; return 0; }
void validation_clear_rules(ValidationContext *ctx) { (void)ctx; }
const char* validation_error_string(Result rc) { (void)rc; return ""; }
void validation_context_init(ValidationContext *ctx) { if (ctx) memset(ctx,0,sizeof(*ctx)); }
void validation_context_free(ValidationContext *ctx) { (void)ctx; }
