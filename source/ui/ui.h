#ifndef HELLO_UI_H
#define HELLO_UI_H

#include "install.h"
#include "secure_validation.h"
#include <stddef.h>

// Security UI elements
typedef enum {
    SECURITY_NORMAL = 0,
    SECURITY_WARNING,
    SECURITY_ERROR,
    SECURITY_CRITICAL
} SecurityLevel;

typedef struct {
    const char *title;
    const char *message;
    SecurityLevel level;
    bool require_confirmation;
    bool show_checkbox;
    const char *checkbox_text;
} SecurityPrompt;

// Enhanced page types
typedef enum {
    PAGE_MAIN_MENU = 0,
    PAGE_FILE_BROWSER,
    PAGE_DOWNLOADS,
    PAGE_SETTINGS,
    PAGE_THEMES,
    PAGE_TEXT_VIEW,
    PAGE_SECURITY_AUDIT,    // New security audit page
    PAGE_SECURITY_SETTINGS, // New security settings page
    PAGE_VALIDATION_REPORT  // New validation report page
} AppPage;

// Render core views
void render_text_view(int top_row, int selected_row, const char **lines, 
                     int total_lines, int view_rows, int view_cols);
void render_active_view(int top_row, int selected_row, AppPage page, 
                       char **lines_buf, int total_lines, 
                       int view_rows, int view_cols);
void show_install_list(int gr, InstallItem *items, int count, int selected);

// Security UI functions
Result ui_show_security_prompt(const SecurityPrompt *prompt, bool *result);
void ui_show_security_banner(const char *message, SecurityLevel level);
void ui_render_security_status(int x, int y, SecurityLevel level);
void ui_render_validation_status(int x, int y, ValidationFlags status);
void ui_show_audit_report(const AuditReport *report);
Result ui_render_security_settings(void);

// Enhanced install UI with security
void render_install_detail(const InstallItem *item, int view_rows, int view_cols);
Result ui_confirm_risky_operation(const char *operation, const char *details);
void ui_show_validation_progress(const char *file, size_t current, size_t total);
void ui_update_security_indicators(void);

// Security-focused menus
extern const char *g_security_menu[];
extern const int g_security_menu_count;
extern const char *g_dumps_menu[];
extern const int g_dumps_count;
extern const char *g_menu_items[];
extern const int g_menu_count;

#endif // HELLO_UI_H
