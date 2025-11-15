#ifndef HELLO_SETTINGS_H
#define HELLO_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

// App modes
typedef enum {
    APP_MODE_NORMAL = 0,
    APP_MODE_BATTERY_SAVER,
    APP_MODE_STORAGE_SAVER,
    APP_MODE_EFFICIENT
} AppMode;

typedef struct {
    char theme[64];
    char download_dir[256];
    int confirm_installs;
    char language[16];

    AppMode app_mode;
    int auto_mode_enabled; // auto-switch based on battery/storage
    int battery_threshold_percent; // below this triggers Battery Saver
    unsigned long long storage_threshold_bytes; // below this triggers Storage Saver (bytes)

    // Derived flags applied by selected mode
    int disable_rumble;
    int refresh_rate_ms;
    int highlight_large_files;
    int enable_smart_folders;
    int prioritize_plugins;

    // Parental controls
    int parental_enabled;                 // 0 = off, 1 = on
    char parental_pin_hash[128];          // hex PBKDF2 hash
    char parental_pin_salt[64];           // hex salt
    char parental_webhook[256];           // webhook URL (if any)
    int parental_report_days;             // report interval in days
    long parental_last_report;            // epoch seconds of last report
    char parental_contact[128];           // optional parent contact/email
} AppSettings;

extern AppSettings g_settings;

void save_settings(void);
void load_settings(void);
void apply_theme(const char *name);

// Menu helper
void settings_menu(int view_rows, int view_cols);

// Theme sequence accessors (return pointer to ANSI sequence strings)
const char *settings_get_seq_normal(void);
const char *settings_get_seq_highlight(void);
const char *settings_get_language(void);

// Prototype for helper used by parental.c
void settings_mark_parental_report(long epoch_seconds);

// Apply and auto-switch helpers
void settings_check_auto_mode(void);
void settings_apply_mode(int mode);

#endif // HELLO_SETTINGS_H
#ifndef HELLO_SETTINGS_H
#define HELLO_SETTINGS_H

#include <stdbool.h>

typedef struct {
    char theme[64];
    char download_dir[256];
    int confirm_installs;
    char language[16];

// App modes
typedef enum {
    APP_MODE_NORMAL = 0,
    APP_MODE_BATTERY_SAVER,
    APP_MODE_STORAGE_SAVER,
    APP_MODE_EFFICIENT
} AppMode;

    AppMode app_mode;
    int auto_mode_enabled; // auto-switch based on battery/storage
    int battery_threshold_percent; // below this triggers Battery Saver
    unsigned long storage_threshold_bytes; // below this triggers Storage Saver

    // Derived flags applied by selected mode
    int disable_rumble;
    int refresh_rate_ms;
    int highlight_large_files;
    int enable_smart_folders;
    int prioritize_plugins;

    // Parental controls
    int parental_enabled;                 // 0 = off, 1 = on
    char parental_pin_hash[128];          // hex PBKDF2 hash
    char parental_pin_salt[64];           // hex salt
    char parental_webhook[256];           // webhook URL (if any)
    int parental_report_days;             // report interval in days
    long parental_last_report;            // epoch seconds of last report
    char parental_contact[128];           // optional parent contact/email
} AppSettings;

extern AppSettings g_settings;

void save_settings(void);
void load_settings(void);
void apply_theme(const char *name);

// Menu helper
void settings_menu(int view_rows, int view_cols);

// Theme sequence accessors (return pointer to ANSI sequence strings)
const char *settings_get_seq_normal(void);
const char *settings_get_seq_highlight(void);
const char *settings_get_language(void);

// Prototype for helper used by parental.c
void settings_mark_parental_report(long epoch_seconds);

// Apply and auto-switch helpers
void settings_check_auto_mode(void);
void settings_apply_mode(int mode);

#endif // HELLO_SETTINGS_H
