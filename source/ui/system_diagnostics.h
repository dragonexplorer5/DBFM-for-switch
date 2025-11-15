#ifndef SYSTEM_DIAGNOSTICS_H
#define SYSTEM_DIAGNOSTICS_H

#include <switch.h>

// Diagnostic data structure
typedef struct {
    int battery_percent;
    bool is_charging;
    int temperature_mc;
    u64 free_space;
    u64 total_space;
    int current_mode;
    bool auto_mode_enabled;
    int battery_threshold;
    u64 storage_threshold;
} SystemDiagnostics;

// UI Functions
void system_diagnostics_init(void);
void system_diagnostics_exit(void);
void system_diagnostics_show(void);
void system_diagnostics_update(void);

// Safe shutdown support
bool system_should_shutdown(void);
void system_safe_shutdown(const char* reason);

#endif // SYSTEM_DIAGNOSTICS_H