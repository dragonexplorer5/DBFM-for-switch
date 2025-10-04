#ifndef HELLO_UI_H
#define HELLO_UI_H

#include "install.h"
#include <stddef.h>
// Render a single-line-per-row text view. top_row is the index of the first visible line.
void render_text_view(int top_row, int selected_row, const char **lines, int total_lines, int view_rows, int view_cols);

// Render the active page view (main menu, file browser, downloads, settings, themes)
// lines_buf is used for file browser and text view pages where the content comes from a dynamic buffer.
typedef enum {
    PAGE_MAIN_MENU = 0,
    PAGE_FILE_BROWSER,
    PAGE_DOWNLOADS,
    PAGE_SETTINGS,
    PAGE_THEMES,
    PAGE_TEXT_VIEW
} AppPage;

void render_active_view(int top_row, int selected_row, AppPage page, char **lines_buf, int total_lines, int view_rows, int view_cols);
// Show install list helper (exposed for use by main.c)
void show_install_list(int gr, InstallItem *items, int count, int selected);

// Dumps submenu (defined in ui_data.c)
extern const char *g_dumps_menu[];
extern const int g_dumps_count;

// Render a single install item's detail (name + description) using language preference
void render_install_detail(const InstallItem *item, int view_rows, int view_cols);

#endif // HELLO_UI_H

// Export menu data defined in ui_data.c
extern const char *g_menu_items[];
extern const int g_menu_count;
