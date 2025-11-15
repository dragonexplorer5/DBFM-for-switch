#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "ui_data.h"
#include "install.h"
#include "fs.h"
#include <stdarg.h>

// externs provided by main.c or other modules
extern const char *g_menu_items[];
extern const int g_menu_count;
extern const char *g_settings_lines[];
extern const int g_settings_count;
extern const char *g_theme_lines[];
extern const int g_theme_count;

void render_text_view(int top_row, int selected_row, const char **lines, int total_lines, int view_rows, int view_cols) {
    // ensure selected_row is within visible window (caller may also adjust top_row,
    // but make a best-effort here: clamp top_row so selected_row is visible)
    if (selected_row < top_row) top_row = selected_row;
    if (selected_row >= top_row + view_rows) top_row = selected_row - view_rows + 1;
    if (top_row < 0) top_row = 0;
    if (top_row > total_lines - view_rows) top_row = total_lines - view_rows < 0 ? 0 : total_lines - view_rows;

    printf("\x1b[1;1H"); // move to top-left
    for (int r = 0; r < view_rows; ++r) {
        int idx = top_row + r;
        if (idx < 0 || idx >= total_lines) {
            printf("\x1b[K\n");
            continue;
        }
        if (idx == selected_row) printf("\x1b[7m"); else printf("\x1b[0m");
        char buf[1024]; int copy = view_cols < (int)sizeof(buf)-1 ? view_cols : (int)sizeof(buf)-1;
        int len = 0; while (len < copy && lines[idx][len] != '\0') len++;
        memcpy(buf, lines[idx], len); buf[len] = '\0';
        printf("%s\x1b[K\n", buf);
    }
    printf("\x1b[KLine: %d/%d\n", selected_row + 1, total_lines);
    fflush(stdout);
}

void show_install_list(int gr, InstallItem *items, int count, int selected) {
    int start_row = gr + 2;
    printf("\x1b[%d;1H", start_row);
    printf("Available installs (no CFW):\n");
    for (int i = 0; i < count; ++i) {
        const char *status = items[i].installed ? "Installed (up-to-date)" : "Missing";
        if (i == selected) printf(" > "); else printf("   ");
        printf("%s : %s\n", items[i].name, status);
    }
    fflush(stdout);
}

void render_install_detail(const InstallItem *item, int view_rows, int view_cols) {
    int start_row = view_rows + 2;
    printf("\x1b[%d;1H", start_row);
    printf("Name: %s\n", item->name);
    // choose description based on settings language
    extern const char *settings_get_language(void);
    const char *lang = settings_get_language();
    const char *desc = item->desc;
    if (lang && strcmp(lang, "en") == 0 && item->desc_en && item->desc_en[0]) desc = item->desc_en;
    printf("%s\n", desc ? desc : "(no description)");
    fflush(stdout);
}

void render_active_view(int top_row, int selected_row, AppPage page, char **lines_buf, int total_lines, int view_rows, int view_cols) {
    switch (page) {
        case PAGE_MAIN_MENU:
            // New homescreen layout (grid + top bar)
            render_homescreen(top_row, selected_row, view_rows, view_cols);
            break;
        case PAGE_FILE_BROWSER:
            render_text_view(top_row, selected_row, (const char **)lines_buf, total_lines, view_rows, view_cols);
            break;
        case PAGE_DOWNLOADS:
            show_install_list(view_rows, g_candidates, g_candidate_count, selected_row);
            break;
        case PAGE_SETTINGS:
            render_text_view(top_row, selected_row, g_settings_lines, g_settings_count, view_rows, view_cols);
            break;
        case PAGE_THEMES:
            render_text_view(top_row, selected_row, g_theme_lines, g_theme_count, view_rows, view_cols);
            break;
        case PAGE_TEXT_VIEW:
        default:
            render_text_view(top_row, selected_row, (const char **)lines_buf, total_lines, view_rows, view_cols);
            break;
    }
}

int ui_show_menu(const char *title, MenuItem *items, int count) {
    // Very small, portable console fallback for menu selection.
    // Returns the selected index (0-based). If nothing selected, returns -1.
    if (!items || count <= 0) return -1;
    printf("%s\n", title ? title : "Menu");
    for (int i = 0; i < count; ++i) {
        printf("%2d: %s %s\n", i, items[i].text ? items[i].text : "(nil)", items[i].enabled ? "" : "(disabled)");
    }
    // Simple default: return first enabled item
    for (int i = 0; i < count; ++i) if (items[i].enabled) return i;
    return 0;
}

void ui_show_message(const char *title, const char *fmt, ...) {
    (void)title;
    if (!fmt) return;
    va_list ap; va_start(ap, fmt);
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}

void ui_show_error(const char *title, const char *fmt, ...) {
    (void)title;
    if (!fmt) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

void ui_set_status(const char *status) {
    if (!status) return;
    printf("Status: %s\n", status);
    fflush(stdout);
}

int ui_show_dialog(const char *title, const char *message) {
    (void)title;
    if (!message) return 0;
    printf("%s\n", message);
    // Simple default: return true to confirm
    return 1;
}

int ui_show_keyboard(const char *title, char *buf, size_t buf_len) {
    (void)title;
    if (!buf || buf_len == 0) return 0;
    // Minimal keyboard: read a line from stdin
    if (!fgets(buf, (int)buf_len, stdin)) return 0;
    // strip newline
    size_t L = strlen(buf);
    if (L > 0 && buf[L-1] == '\n') buf[L-1] = '\0';
    return 1;
}
