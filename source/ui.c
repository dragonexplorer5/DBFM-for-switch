#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "install.h"
#include "fs.h"

// externs provided by main.c or other modules
extern const char *g_menu_items[];
extern const int g_menu_count;
extern const char *g_settings_lines[];
extern const int g_settings_count;
extern const char *g_theme_lines[];
extern const int g_theme_count;

void render_text_view(int top_row, int selected_row, const char **lines, int total_lines, int view_rows, int view_cols) {
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
            render_text_view(top_row, selected_row, g_menu_items, g_menu_count, view_rows, view_cols);
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
