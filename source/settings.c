#include "settings.h"
#include <switch.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include "system/system_manager.h"

static const char *settings_path = "sdmc:/switch/hello-world/settings.cfg";

AppSettings g_settings = {0};

// Test auto-mode switching behavior
bool test_auto_mode_switching(void) {
    int prev_mode = g_settings.app_mode;
    int battery = system_get_battery_percent();
    bool test_passed = true;
    
    system_log(SYSTEM_LOG_INFO, "Testing auto-mode switching...");
    system_log(SYSTEM_LOG_INFO, "Current battery: %d%%", battery);
    
    // Test 1: Battery threshold
    if (battery != -1 && battery < g_settings.battery_threshold_percent) {
        settings_check_auto_mode();
        if (g_settings.app_mode != APP_MODE_BATTERY_SAVER) {
            system_log(SYSTEM_LOG_ERROR, "Battery saver not triggered at %d%% (threshold: %d%%)",
                      battery, g_settings.battery_threshold_percent);
            test_passed = false;
        }
    }
    
    // Test 2: Storage threshold
    u64 free_space = 0;
    if (R_SUCCEEDED(system_get_free_space(NAND_PARTITION_USER, &free_space))) {
        system_log(SYSTEM_LOG_INFO, "Free space: %llu bytes", free_space);
        if (free_space < g_settings.storage_threshold_bytes) {
            settings_check_auto_mode();
            if (g_settings.app_mode != APP_MODE_STORAGE_SAVER) {
                system_log(SYSTEM_LOG_ERROR, "Storage saver not triggered at %llu bytes (threshold: %llu)",
                          free_space, (unsigned long long)g_settings.storage_threshold_bytes);
                test_passed = false;
            }
        }
    }
    
    // Test 3: Mode application
    AppMode test_modes[] = {
        APP_MODE_NORMAL,
        APP_MODE_BATTERY_SAVER,
        APP_MODE_STORAGE_SAVER,
        APP_MODE_EFFICIENT
    };
    
    for (int i = 0; i < 4; i++) {
        settings_apply_mode(test_modes[i]);
        if (g_settings.app_mode != test_modes[i]) {
            system_log(SYSTEM_LOG_ERROR, "Failed to apply mode %d", test_modes[i]);
            test_passed = false;
        }
        
        // Verify derived flags
        if (test_modes[i] == APP_MODE_BATTERY_SAVER) {
            if (!g_settings.disable_rumble || g_settings.refresh_rate_ms < 500) {
                system_log(SYSTEM_LOG_ERROR, "Battery saver flags not applied correctly");
                test_passed = false;
            }
        }
    }
    
    // Restore original mode
    settings_apply_mode(prev_mode);
    return test_passed;
}

void save_settings(void) {
    FILE *f = fopen(settings_path, "w");
    if (!f) return;
    fprintf(f, "theme=%s\n", g_settings.theme[0] ? g_settings.theme : "default");
    fprintf(f, "download_dir=%s\n", g_settings.download_dir[0] ? g_settings.download_dir : "sdmc:/switch/.tmp");
    fprintf(f, "confirm_installs=%d\n", g_settings.confirm_installs);
    fprintf(f, "app_mode=%d\n", g_settings.app_mode);
    fprintf(f, "auto_mode_enabled=%d\n", g_settings.auto_mode_enabled);
    fprintf(f, "battery_threshold_percent=%d\n", g_settings.battery_threshold_percent);
    fprintf(f, "storage_threshold_bytes=%llu\n", (unsigned long long)g_settings.storage_threshold_bytes);
    fprintf(f, "language=%s\n", g_settings.language[0] ? g_settings.language : "en");
    fclose(f);
}

void load_settings(void) {
    FILE *f = fopen(settings_path, "r");
    if (!f) {
        // defaults
        strcpy(g_settings.theme, "default");
        strcpy(g_settings.download_dir, "sdmc:/switch/.tmp");
        g_settings.confirm_installs = 1;
        g_settings.app_mode = APP_MODE_NORMAL;
        g_settings.auto_mode_enabled = 1;
        g_settings.battery_threshold_percent = 20; // percent
        g_settings.storage_threshold_bytes = 2ULL * 1024ULL * 1024ULL * 1024ULL; // 2GB
        strcpy(g_settings.language, "en");
        return;
    }
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '='); if (!eq) continue; *eq = '\0'; char *key = line; char *val = eq + 1;
        char *nl = strchr(val, '\n'); if (nl) *nl = '\0';
        if (strcmp(key, "theme") == 0) strncpy(g_settings.theme, val, sizeof(g_settings.theme)-1);
        else if (strcmp(key, "download_dir") == 0) strncpy(g_settings.download_dir, val, sizeof(g_settings.download_dir)-1);
        else if (strcmp(key, "confirm_installs") == 0) g_settings.confirm_installs = atoi(val);
        else if (strcmp(key, "app_mode") == 0) g_settings.app_mode = (AppMode)atoi(val);
        else if (strcmp(key, "auto_mode_enabled") == 0) g_settings.auto_mode_enabled = atoi(val);
        else if (strcmp(key, "battery_threshold_percent") == 0) g_settings.battery_threshold_percent = atoi(val);
    else if (strcmp(key, "storage_threshold_bytes") == 0) g_settings.storage_threshold_bytes = strtoull(val, NULL, 10);
        else if (strcmp(key, "language") == 0) strncpy(g_settings.language, val, sizeof(g_settings.language)-1);
    }
    fclose(f);

    // Apply derived flags for the currently selected mode
    switch (g_settings.app_mode) {
        case APP_MODE_BATTERY_SAVER:
            g_settings.disable_rumble = 1;
            g_settings.refresh_rate_ms = 100;
            g_settings.highlight_large_files = 0;
            g_settings.enable_smart_folders = 0;
            g_settings.prioritize_plugins = 0;
            break;
        case APP_MODE_STORAGE_SAVER:
            g_settings.disable_rumble = 0;
            g_settings.refresh_rate_ms = 33;
            g_settings.highlight_large_files = 1;
            g_settings.enable_smart_folders = 0;
            g_settings.prioritize_plugins = 0;
            break;
        case APP_MODE_EFFICIENT:
            g_settings.disable_rumble = 0;
            g_settings.refresh_rate_ms = 16;
            g_settings.highlight_large_files = 0;
            g_settings.enable_smart_folders = 1;
            g_settings.prioritize_plugins = 1;
            break;
        case APP_MODE_NORMAL:
        default:
            g_settings.disable_rumble = 0;
            g_settings.refresh_rate_ms = 16;
            g_settings.highlight_large_files = 0;
            g_settings.enable_smart_folders = 0;
            g_settings.prioritize_plugins = 0;
            break;
    }
}

void settings_apply_mode(int mode) {
    g_settings.app_mode = (AppMode)mode;
    switch (g_settings.app_mode) {
        case APP_MODE_BATTERY_SAVER:
            strcpy(g_settings.theme, "dark");
            g_settings.disable_rumble = 1;
            g_settings.refresh_rate_ms = 100;
            g_settings.highlight_large_files = 0;
            g_settings.enable_smart_folders = 0;
            g_settings.prioritize_plugins = 0;
            break;
        case APP_MODE_STORAGE_SAVER:
            strcpy(g_settings.theme, "default");
            g_settings.disable_rumble = 0;
            g_settings.refresh_rate_ms = 33;
            g_settings.highlight_large_files = 1;
            g_settings.enable_smart_folders = 0;
            g_settings.prioritize_plugins = 0;
            break;
        case APP_MODE_EFFICIENT:
            strcpy(g_settings.theme, "default");
            g_settings.disable_rumble = 0;
            g_settings.refresh_rate_ms = 16;
            g_settings.highlight_large_files = 0;
            g_settings.enable_smart_folders = 1;
            g_settings.prioritize_plugins = 1;
            break;
        case APP_MODE_NORMAL:
        default:
            strcpy(g_settings.theme, "default");
            g_settings.disable_rumble = 0;
            g_settings.refresh_rate_ms = 16;
            g_settings.highlight_large_files = 0;
            g_settings.enable_smart_folders = 0;
            g_settings.prioritize_plugins = 0;
            break;
    }
    apply_theme(g_settings.theme);
    save_settings();
}

void settings_check_auto_mode(void) {
    if (!g_settings.auto_mode_enabled) return;

    // Check battery first (higher priority)
    int batt = system_get_battery_percent();
    if (batt >= 0) {
        if (batt <= g_settings.battery_threshold_percent) {
            if (g_settings.app_mode != APP_MODE_BATTERY_SAVER) settings_apply_mode(APP_MODE_BATTERY_SAVER);
            return;
        }
    }

    // Check storage
    u64 free_bytes = 0;
    if (R_SUCCEEDED(system_get_free_space(NAND_PARTITION_USER, &free_bytes))) {
        if (free_bytes <= g_settings.storage_threshold_bytes) {
            if (g_settings.app_mode != APP_MODE_STORAGE_SAVER) settings_apply_mode(APP_MODE_STORAGE_SAVER);
            return;
        }
    }

    // No condition matched: revert to normal if currently in an auto mode
    if (g_settings.app_mode == APP_MODE_BATTERY_SAVER || g_settings.app_mode == APP_MODE_STORAGE_SAVER) {
        settings_apply_mode(APP_MODE_NORMAL);
    }
}

void apply_theme(const char *name) {
    (void)name; // for now the theme name is stored; UI uses accessors to query sequences
    // Could map named themes to different sequences here in future
}

// central ANSI sequences for themes
static char seq_normal[32] = "\x1b[0m";
static char seq_highlight[32] = "\x1b[7m";

const char *settings_get_seq_normal(void) { return seq_normal; }
const char *settings_get_seq_highlight(void) { return seq_highlight; }

const char *settings_get_language(void) { return g_settings.language; }

void settings_mark_parental_report(long epoch_seconds) {
    g_settings.parental_last_report = epoch_seconds;
    // Persist settings immediately
    save_settings();
}

// Simple settings menu: toggle confirm_installs and cycle theme list
void settings_menu(int view_rows, int view_cols) {
    const char *options[] = { "Confirm installs", "Theme", "App Mode", "Auto Mode (battery/storage)", "Battery threshold", "Storage threshold (bytes)", "Save and return" };
    const int opt_count = sizeof(options) / sizeof(options[0]);
    int sel = 0;
    // Clear the screen before rendering settings UI to avoid leftover text
    consoleClear();
    PadState pad; padInitializeDefault(&pad); padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    while (appletMainLoop()) {
        // render
        printf("\x1b[1;1H");
        for (int i = 0; i < opt_count; ++i) {
            if (i == sel) printf("> %s\n", options[i]); else printf("  %s\n", options[i]);
        }
        const char *mode_names[] = { "Normal", "Battery Saver", "Storage Saver", "Efficient" };
    printf("\nCurrent: Confirm=%d Theme=%s Mode=%s Auto=%d BatThr=%d%% StorThr=%llu\n", g_settings.confirm_installs, g_settings.theme, mode_names[g_settings.app_mode], g_settings.auto_mode_enabled, g_settings.battery_threshold_percent, (unsigned long long)g_settings.storage_threshold_bytes);
        fflush(stdout);

        padUpdate(&pad); u64 kd = padGetButtonsDown(&pad);
        if (kd & HidNpadButton_Down) sel = (sel + 1) % opt_count;
        if (kd & HidNpadButton_Up) sel = (sel - 1 + opt_count) % opt_count;
        if (kd & HidNpadButton_A) {
            if (sel == 0) {
                g_settings.confirm_installs = !g_settings.confirm_installs; save_settings();
            } else if (sel == 1) {
                if (strcmp(g_settings.theme, "default") == 0) strcpy(g_settings.theme, "dark");
                else if (strcmp(g_settings.theme, "dark") == 0) strcpy(g_settings.theme, "blue");
                else strcpy(g_settings.theme, "default");
                apply_theme(g_settings.theme); save_settings();
            } else if (sel == 2) {
                // cycle app mode
                g_settings.app_mode = (g_settings.app_mode + 1) % 4;
                save_settings();
            } else if (sel == 3) {
                g_settings.auto_mode_enabled = !g_settings.auto_mode_enabled; save_settings();
            } else if (sel == 4) {
                // increase battery threshold by 5%
                g_settings.battery_threshold_percent += 5; if (g_settings.battery_threshold_percent > 80) g_settings.battery_threshold_percent = 80; save_settings();
            } else if (sel == 5) {
                // toggle storage threshold presets
                if (g_settings.storage_threshold_bytes == 2ULL * 1024ULL * 1024ULL * 1024ULL) g_settings.storage_threshold_bytes = 5ULL * 1024ULL * 1024ULL * 1024ULL;
                else if (g_settings.storage_threshold_bytes == 5ULL * 1024ULL * 1024ULL * 1024ULL) g_settings.storage_threshold_bytes = 1ULL * 1024ULL * 1024ULL * 1024ULL;
                else g_settings.storage_threshold_bytes = 2ULL * 1024ULL * 1024ULL * 1024ULL;
                save_settings();
            } else { break; }
        }
        if (kd & HidNpadButton_B) break;
        consoleUpdate(NULL);
    }
}
