#include "system_diagnostics.h"
#include "../system/system_manager.h"
#include "../settings.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>
#include "system_diagnostics.h"
#include "../system/system_manager.h"
#include "../settings.h"
#include "ui.h"
#include <stdio.h>

static SystemDiagnostics diagnostics = {0};
static bool shutdown_requested = false;
static char shutdown_reason[256] = {0};

static const char* get_mode_string(int mode) {
    switch (mode) {
        case APP_MODE_NORMAL: return "Normal";
        case APP_MODE_BATTERY_SAVER: return "Battery Saver";
        case APP_MODE_STORAGE_SAVER: return "Storage Saver";
        case APP_MODE_EFFICIENT: return "Efficient";
        default: return "Unknown";
    }
}

void system_diagnostics_init(void) {
    shutdown_requested = false;
    memset(&diagnostics, 0, sizeof(diagnostics));
}

void system_diagnostics_exit(void) {
    // Nothing to clean up yet
}

static void update_diagnostics(void) {
    // Battery info
    diagnostics.battery_percent = system_get_battery_percent();
    
    // Temperature (convert to celsius for display)
    diagnostics.temperature_mc = system_get_temperature();
    
    // Storage info
        // Use user partition for diagnostics (simplified)
        (void)system_get_free_space(NAND_PARTITION_USER, &diagnostics.free_space);
        (void)system_get_total_space(NAND_PARTITION_USER, &diagnostics.total_space);
    
    // App mode info
    diagnostics.current_mode = g_settings.app_mode;
    diagnostics.auto_mode_enabled = g_settings.auto_mode_enabled;
    diagnostics.battery_threshold = g_settings.battery_threshold_percent;
    diagnostics.storage_threshold = g_settings.storage_threshold_bytes;

    // Check for critical temperature
    if (diagnostics.temperature_mc > SYSTEM_TEMP_CRITICAL) {
        system_safe_shutdown("Critical temperature threshold exceeded");
    }
}

void system_diagnostics_show(void) {
    update_diagnostics();
    
    while (true) {
        ui_begin_frame();
        
        ui_header("System Diagnostics");
        
        // Battery section
        ui_header_sub("Battery Status");
        if (diagnostics.battery_percent >= 0) {
            ui_label("Battery Level: %d%%", diagnostics.battery_percent);
            if (diagnostics.battery_percent <= diagnostics.battery_threshold) {
                ui_label_warning("Below battery threshold (%d%%)", diagnostics.battery_threshold);
            }
        } else {
            ui_label_error("Battery status unavailable");
        }
        
        // Temperature section with color coding
        ui_header_sub("Temperature");
        float temp_c = diagnostics.temperature_mc / 1000.0f;
        if (diagnostics.temperature_mc > 0) {
            if (diagnostics.temperature_mc >= SYSTEM_TEMP_CRITICAL) {
                ui_label_error("CRITICAL: %.1f°C", temp_c);
            } else if (diagnostics.temperature_mc >= SYSTEM_TEMP_WARNING) {
                ui_label_warning("WARNING: %.1f°C", temp_c);
            } else {
                ui_label("Normal: %.1f°C", temp_c);
            }
        } else {
            ui_label_error("Temperature sensor unavailable");
        }
        
        // Storage section
        ui_header_sub("Storage");
        float free_gb = diagnostics.free_space / (1024.0f * 1024.0f * 1024.0f);
        float total_gb = diagnostics.total_space / (1024.0f * 1024.0f * 1024.0f);
        ui_label("Free Space: %.1f GB / %.1f GB", free_gb, total_gb);
        float threshold_gb = diagnostics.storage_threshold / (1024.0f * 1024.0f * 1024.0f);
        if (free_gb < threshold_gb) {
            ui_label_warning("Below storage threshold (%.1f GB)", threshold_gb);
        }
        
        // Mode section
        ui_header_sub("App Mode");
        ui_label("Current Mode: %s", get_mode_string(diagnostics.current_mode));
        ui_label("Auto-switching: %s", diagnostics.auto_mode_enabled ? "Enabled" : "Disabled");
        
        if (ui_button("Back")) {
            break;
        }
        
        ui_end_frame();
        
        if (shutdown_requested) {
            ui_show_message("SYSTEM SHUTDOWN", shutdown_reason);
            svcSleepThread(3000000000ULL); // 3 second delay
            system_safe_shutdown(shutdown_reason);
            break;
        }
    }
}

void system_diagnostics_update(void) {
    update_diagnostics();
}

bool system_should_shutdown(void) {
    return shutdown_requested;
}

void system_safe_shutdown(const char* reason) {
    if (!shutdown_requested) {
        shutdown_requested = true;
        strncpy(shutdown_reason, reason, sizeof(shutdown_reason)-1);
        system_log(SYSTEM_LOG_ERROR, "Safe shutdown triggered: %s", reason);
        
        // Save settings before shutdown
        save_settings();
        
        // Flush any pending writes
        fsdevCommitDevice("sdmc");
        
#ifdef __SWITCH__
        // Tell the OS we're ready to close
        appletSetMediaPlaybackState(false);
    svcExitProcess();
#endif
    }
}