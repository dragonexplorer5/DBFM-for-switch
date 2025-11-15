#ifndef HELLO_UI_H
#define HELLO_UI_H

#include "install.h"
#include "secure_validation.h"
#include <stddef.h>

// Simple MenuItem used across the project. Older code used this shape
// so provide it here for compatibility.
typedef struct {
    const char *text;    /* menu text */
    bool enabled;        /* whether selectable */
} MenuItem;

// Basic interactive helpers (compatibility prototypes). Implementations
// are provided in `ui.c` to allow higher-level code to compile.
int ui_show_menu(const char *title, MenuItem *items, int count);
// Variadic printf-like helpers used widely across the codebase.
void ui_show_message(const char *title, const char *fmt, ...);
void ui_show_error(const char *title, const char *fmt, ...);
// Simple text keyboard input helper (returns true if input accepted)
int ui_keyboard_input(int view_rows, const char *prompt, char *buf, size_t buf_len);
// Update status text shown in UI (compatibility helper)
void ui_set_status(const char *status);
// Simple dialog and keyboard helpers used by feature UIs
int ui_show_dialog(const char *title, const char *message);
int ui_show_keyboard(const char *title, char *buf, size_t buf_len);

// Forward declare AuditReport to avoid including heavy audit headers in UI header
struct AuditReport;

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

/* Immediate-mode UI helpers expected by several legacy modules */
void ui_begin_frame(void);
void ui_end_frame(void);
void ui_header(const char *title);
void ui_header_sub(const char *subtitle);
bool ui_button(const char *label);
void ui_label(const char *fmt, ...);
void ui_label_warning(const char *fmt, ...);
void ui_label_error(const char *fmt, ...);

// Security UI functions
Result ui_show_security_prompt(const SecurityPrompt *prompt, bool *result);
void ui_show_security_banner(const char *message, SecurityLevel level);
void ui_render_security_status(int x, int y, SecurityLevel level);
void ui_render_validation_status(int x, int y, ValidationFlags status);
void ui_show_audit_report(const struct AuditReport *report);
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
