#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <stdarg.h>
#include <sys/stat.h>
#include "ui_data.h"
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

void ui_state_init(UIState* state) {
    memset(state, 0, sizeof(UIState));
    state->menu_items = NULL;
    state->menu_item_count = 0;
    state->selected_index = 0;
    state->scroll_offset = 0;
    state->show_help = true;
}

void ui_state_set_menu(UIState* state, const char** items, int count) {
    // Free any existing menu items
    if (state->menu_items) {
        for (int i = 0; i < state->menu_item_count; i++) {
            if (state->menu_items[i]) {
                free(state->menu_items[i]);
            }
        }
        free(state->menu_items);
    }

    state->menu_items = malloc(count * sizeof(char*));
    if (!state->menu_items) {
        state->menu_item_count = 0;
        return;
    }

    state->menu_item_count = count;
    state->selected_index = 0; // Reset selection

    // Copy each string
    for (int i = 0; i < count; i++) {
        size_t len = strlen(items[i]) + 1;
        state->menu_items[i] = malloc(len);
        if (state->menu_items[i]) {
            strncpy(state->menu_items[i], items[i], len);
        }
    }
}

// Append a small timestamped message to sdmc:/dbfm/logs/ui_debug.txt so
// maintainers can collect UI init/paint traces from a device without a
// console capture. Directory creation is best-effort.
static void write_ui_log(const char *fmt, ...) {
    mkdir("sdmc:/dbfm", 0777);
    mkdir("sdmc:/dbfm/logs", 0777);
    FILE *f = fopen("sdmc:/dbfm/logs/ui_debug.txt", "a");
    if (!f) return;
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    if (lt) {
        char ts[64];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", lt);
        fprintf(f, "%s - ", ts);
    }
    va_list ap; va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\n");
    fclose(f);
}

// Downloads queue (simple FIFO list)
#define MAX_DOWNLOADS 16
typedef struct {
    char label[128];
    int progress; // 0-100
    bool active;
} DownloadEntry;

static DownloadEntry g_downloads[MAX_DOWNLOADS];

static int downloads_count(void) {
    int c = 0; for (int i = 0; i < MAX_DOWNLOADS; ++i) if (g_downloads[i].active) c++; return c;
}

static void downloads_push_or_update(const char *label, int progress) {
    if (!label) return;
    // find existing
    for (int i = 0; i < MAX_DOWNLOADS; ++i) {
        if (g_downloads[i].active && strcmp(g_downloads[i].label, label) == 0) { g_downloads[i].progress = progress; return; }
    }
    // find free slot
    for (int i = 0; i < MAX_DOWNLOADS; ++i) {
        if (!g_downloads[i].active) { strncpy(g_downloads[i].label, label, sizeof(g_downloads[i].label)-1); g_downloads[i].label[sizeof(g_downloads[i].label)-1] = '\0'; g_downloads[i].progress = progress; g_downloads[i].active = true; return; }
    }
    // no space: replace oldest (slot 0)
    strncpy(g_downloads[0].label, label, sizeof(g_downloads[0].label)-1); g_downloads[0].label[sizeof(g_downloads[0].label)-1] = '\0'; g_downloads[0].progress = progress; g_downloads[0].active = true;
}

// Public wrappers for other modules
void ui_downloads_push_update(const char *label, int progress) {
    downloads_push_or_update(label, progress < 0 ? 0 : (progress > 100 ? 100 : progress));
    // Also update the global task area when active
    if (label && progress >= 0) {
        char taskbuf[128]; snprintf(taskbuf, sizeof(taskbuf), "Downloading: %s", label);
        ui_set_task(taskbuf, progress);
    } else if (label && progress < 0) {
        ui_downloads_remove(label);
    }
}

void ui_downloads_remove(const char *label) {
    if (!label) return;
    for (int i = 0; i < MAX_DOWNLOADS; ++i) {
        if (g_downloads[i].active && strcmp(g_downloads[i].label, label) == 0) {
            g_downloads[i].active = false;
            g_downloads[i].label[0] = '\0';
            g_downloads[i].progress = 0;
            break;
        }
    }
    // Clear task area when no more downloads active
    if (downloads_count() == 0) ui_clear_task();
}

static void downloads_pop_front(void) {
    // remove first active and compact
    int first = -1; for (int i = 0; i < MAX_DOWNLOADS; ++i) if (g_downloads[i].active) { first = i; break; }
    if (first == -1) return;
    // shift
    for (int i = first; i < MAX_DOWNLOADS-1; ++i) g_downloads[i] = g_downloads[i+1];
    // clear last
    g_downloads[MAX_DOWNLOADS-1].active = false; g_downloads[MAX_DOWNLOADS-1].label[0] = '\0'; g_downloads[MAX_DOWNLOADS-1].progress = 0;
}

// Favorites persistence (simple label list, one per line)
#define FAVORITES_PATH "sdmc:/dbfm/favorites.txt"
#define MAX_FAVORITES 64
static char g_favorites[MAX_FAVORITES][64];
static int g_favorites_count = 0;

Result ui_favorites_load(void) {
    g_favorites_count = 0;
    FILE *f = fopen(FAVORITES_PATH, "r");
    if (!f) return (Result)-1;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        // trim newline and whitespace
        int L = (int)strlen(line);
        while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r' || isspace((unsigned char)line[L-1])) ) { line[--L] = '\0'; }
        if (L == 0) continue;
        if (g_favorites_count < MAX_FAVORITES) { strncpy(g_favorites[g_favorites_count], line, sizeof(g_favorites[g_favorites_count])-1); g_favorites[g_favorites_count][sizeof(g_favorites[g_favorites_count])-1] = '\0'; g_favorites_count++; }
    }
    fclose(f);
    return 0;
}

Result ui_favorites_save(void) {
    FILE *f = fopen(FAVORITES_PATH, "w");
    if (!f) return (Result)-1;
    for (int i = 0; i < g_favorites_count; ++i) {
        fprintf(f, "%s\n", g_favorites[i]);
    }
    fclose(f);
    return 0;
}

void ui_favorites_toggle(const char *label) {
    if (!label) return;
    // check existing
    for (int i = 0; i < g_favorites_count; ++i) {
        if (strcmp(g_favorites[i], label) == 0) {
            // remove
            for (int j = i; j < g_favorites_count - 1; ++j) strcpy(g_favorites[j], g_favorites[j+1]);
            g_favorites_count--; ui_favorites_save(); return;
        }
    }
    if (g_favorites_count < MAX_FAVORITES) {
        strncpy(g_favorites[g_favorites_count], label, sizeof(g_favorites[g_favorites_count])-1); g_favorites[g_favorites_count][sizeof(g_favorites[g_favorites_count])-1] = '\0'; g_favorites_count++; ui_favorites_save();
    }
}

bool ui_favorites_contains(const char *label) {
    if (!label) return false;
    for (int i = 0; i < g_favorites_count; ++i) if (strcmp(g_favorites[i], label) == 0) return true;
    return false;
}

int ui_favorites_count(void) {
    return g_favorites_count;
}

const char *ui_favorites_get(int idx) {
    if (idx < 0 || idx >= g_favorites_count) return NULL;
    return g_favorites[idx];
}

// Terminal probe: use ANSI device status report as implemented earlier
bool ui_probe_terminal_size(int *out_rows, int *out_cols) {
    if (!out_rows || !out_cols) return false;
    printf("\x1b[999;999H");
    printf("\x1b[6n");
    fflush(stdout);
    char buf[64]; int idx = 0;
    struct timeval tv = {0, 200000};
    int fd = fileno(stdin);
    fd_set rfds;
    while (idx < (int)sizeof(buf)-1) {
        FD_ZERO(&rfds); FD_SET(fd, &rfds);
        if (select(fd+1, &rfds, NULL, NULL, &tv) <= 0) break;
        int ch = getchar(); if (ch == EOF) break; buf[idx++] = (char)ch; if (ch == 'R') break; tv.tv_sec = 0; tv.tv_usec = 100000;
    }
    buf[idx] = '\0';
    if (idx <= 0) return false;
    char *p = strstr(buf, "\x1b["); if (!p) return false;
    int r=0,c=0; if (sscanf(p, "\x1b[%d;%dR", &r, &c) != 2) return false; if (r <= 0 || c <= 0) return false; *out_rows = r; *out_cols = c; return true;
}

// Show minimal downloads queue UI
void ui_show_downloads_queue(int view_rows, int view_cols) {
    // Build active list snapshot (indices)
    int active_idx[MAX_DOWNLOADS]; int active_count = 0;
    for (int i = 0; i < MAX_DOWNLOADS; ++i) if (g_downloads[i].active) active_idx[active_count++] = i;

    consoleClear();
    PadState pad; padInitializeDefault(&pad);
    int sel = 0; if (active_count == 0) sel = -1;

    while (appletMainLoop()) {
        // Render
        consoleClear();
        printf("Downloads Queue:\n\n");
        if (active_count == 0) {
            printf("(no active downloads)\n");
        } else {
            int start_row = 2;
            int max_rows = view_rows - 6; if (max_rows < 3) max_rows = 3;
            int top = 0;
            // ensure selection visible
            if (sel < top) top = sel;
            if (sel >= top + max_rows) top = sel - max_rows + 1;

            for (int r = 0; r < max_rows; ++r) {
                int idx = top + r;
                if (idx >= active_count) { printf("\n"); continue; }
                int gi = active_idx[idx];
                if (idx == sel) printf("\x1b[7m"); else printf("\x1b[0m");
                char labelbuf[64]; snprintf(labelbuf, sizeof(labelbuf), "%s", g_downloads[gi].label);
                // clamp label to view_cols - progress area
                int labw = view_cols - 20; if (labw < 10) labw = 10;
                if ((int)strlen(labelbuf) > labw) labelbuf[labw-3] = '\0', labelbuf[labw-2] = labelbuf[labw-1] = '.';
                printf(" %-*s ", labw, labelbuf);
                // progress bar
                int barw = 12;
                int filled = (g_downloads[gi].progress * barw) / 100;
                printf("[");
                for (int b = 0; b < barw; ++b) putchar(b < filled ? '=' : ' ');
                printf("] %3d%%\n", g_downloads[gi].progress);
            }
        }

        printf("\nA: Details  B: Back  ↑/↓: Navigate\n");
        fflush(stdout);

        // Input
        padUpdate(&pad); u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_B) break;
        if (active_count > 0) {
            if (kDown & HidNpadButton_Up) { if (sel > 0) sel--; }
            if (kDown & HidNpadButton_Down) { if (sel < active_count - 1) sel++; }
            if (kDown & HidNpadButton_A) {
                // Show simple details for selected entry
                int gi = active_idx[sel];
                consoleClear();
                printf("Download: %s\n", g_downloads[gi].label);
                printf("Progress: %d%%\n", g_downloads[gi].progress);
                printf("Press B to return\n"); fflush(stdout);
                // wait for B
                while (appletMainLoop()) {
                    padUpdate(&pad); u64 kd = padGetButtonsDown(&pad);
                    if (kd & HidNpadButton_B) break;
                    consoleUpdate(NULL); svcSleepThread(16666666ULL);
                }
            }
        }

        consoleUpdate(NULL);
        svcSleepThread(16666666ULL); // ~60Hz
        // refresh active list snapshot each loop in case entries change
        active_count = 0; for (int i = 0; i < MAX_DOWNLOADS; ++i) if (g_downloads[i].active) active_idx[active_count++] = i;
        if (active_count == 0) sel = -1; if (sel >= active_count) sel = active_count - 1;
    }
}

MenuAction ui_handle_input(UIState* state) {
    static PadState pad;
    padInitializeDefault(&pad);
    padUpdate(&pad);
    u64 kDown = padGetButtonsDown(&pad);
    if (kDown & HidNpadButton_A) {
        return MENU_ACTION_SELECT;
    }
    else if (kDown & HidNpadButton_B) {
        return MENU_ACTION_BACK;
    }
    else if (kDown & HidNpadButton_Y) {
        return MENU_ACTION_REFRESH;
    }
    else if (kDown & HidNpadButton_X) {
        state->show_help = !state->show_help;
    }
    else if (kDown & HidNpadButton_Up) {
        if (state->selected_index > 0) {
            state->selected_index--;
        }
        if (state->selected_index < state->scroll_offset) state->scroll_offset = state->selected_index;
    }
    else if (kDown & HidNpadButton_Down) {
        if (state->selected_index < state->menu_item_count - 1) {
            state->selected_index++;
        }
        // ensure selected visible (renderer computes visible rows)
        int visible_count = 1; // conservative fallback; renderer will manage proper scroll
        if (state->selected_index >= state->scroll_offset + visible_count) state->scroll_offset = state->selected_index - visible_count + 1;
    }
    return MENU_ACTION_NONE;
}

void ui_render_header(const UIState* state) {
    printf("\x1b[0;0H"); // Move cursor to top
    printf("\x1b[7m %s \x1b[0m\n", state->title);
    printf("%s\n\n", state->subtitle);
}

void ui_render_menu(const UIState* state) {
    if (!state->menu_items || state->menu_item_count == 0) return;

    for (int i = 0; i < state->menu_item_count; i++) {
        if (i == state->selected_index) {
            if (state->menu_items[i]) {
                printf("\x1b[7m> %s\x1b[0m\n", state->menu_items[i]);
            } else {
                printf("\x1b[7m> (null)\x1b[0m\n");
            }
        } else {
            if (state->menu_items[i]) {
                printf("  %s\n", state->menu_items[i]);
            } else {
                printf("  (null)\n");
            }
        }
    }
}

void ui_render_help(const UIState* state) {
    if (!state->show_help) return;
    printf("\n\x1b[7m Controls \x1b[0m\n");
    printf("A: Select  B: Back  Y: Refresh  X: Toggle Help\n");
    printf("↑/↓: Navigate\n");
}

void ui_render_progress(const char* operation, int progress) {
    printf("\n%s [", operation);
    int bar_width = 50;
    int filled = (progress * bar_width) / 100;
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) printf("=");
        else printf(" ");
    }
    printf("] %d%%\n", progress);
}

void ui_render_error(const char* error) {
    printf("\x1b[31mError: %s\x1b[0m\n", error);
}

void ui_clear_screen(void) {
    printf("\x1b[2J"); // Clear screen
    printf("\x1b[H");  // Move cursor to top-left
}

#if 0
static const char *get_protocol_string(void) {
#if defined(USE_LIBCURL)
    return "HTTPS/libcurl";
#elif defined(USE_MBEDTLS)
    return "HTTPS/mbedTLS";
#else
    return "HTTP only";
#endif
}
#endif

// Task/progress state shown under the homescreen
static bool g_ui_task_active = false;
static char g_ui_task_label[128] = "";
static int g_ui_task_progress = 0; // percent 0-100

void ui_set_task(const char *label, int progress_percent) {
    if (!label) label = "";
    strncpy(g_ui_task_label, label, sizeof(g_ui_task_label)-1);
    g_ui_task_label[sizeof(g_ui_task_label)-1] = '\0';
    g_ui_task_progress = progress_percent < 0 ? 0 : (progress_percent > 100 ? 100 : progress_percent);
    g_ui_task_active = true;
}

void ui_clear_task(void) {
    g_ui_task_active = false;
    g_ui_task_label[0] = '\0';
    g_ui_task_progress = 0;
}

// Render a modern homescreen: top bar with system info, main action grid (2x3),
// quick actions row, and status area. Uses simple ANSI terminal box rendering.
void render_homescreen(int top_row, int selected_row, int view_rows, int view_cols) {
    (void)top_row;
    // Log start of homescreen render for on-device diagnostics
    write_ui_log("render_homescreen start: rows=%d cols=%d selected=%d", view_rows, view_cols, selected_row);
    ui_clear_screen();

    // Top bar
    char timebuf[64] = "--:--";
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    if (lt) strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M", lt);
    // Title + time (simple left/right layout)
    printf("\x1b[7m DBFM \x1b[0m  %s\n", timebuf);
    printf("\n");

    // Vertical action list: render each item on its own line
    const char *labels[] = {"File Manager","Game Install/Download","Homebrew Store","Save Manager","System Tools","Settings","Search","Downloads","Logs","Themes","News","Favorites"};
    int item_count = sizeof(labels) / sizeof(labels[0]);

    // vertical scroll state
    static int s_scroll_offset = 0;
    static int s_menu_item_count = 0;
    if (s_menu_item_count != item_count) {
        s_menu_item_count = item_count;
        s_scroll_offset = 0;
    }

    int list_top = 3; // row where list starts
    int max_rows = view_rows - list_top - 5; // leave room for status/footer
    if (max_rows < 3) max_rows = 3;

    // adjust scroll so selected visible
    if (selected_row < s_scroll_offset) s_scroll_offset = selected_row;
    if (selected_row >= s_scroll_offset + max_rows) s_scroll_offset = selected_row - max_rows + 1;

    // Render visible slice
    for (int r = 0; r < max_rows; ++r) {
        int idx = s_scroll_offset + r;
        printf("\x1b[%d;1H", list_top + r);
        if (idx >= item_count) { printf("\x1b[0m\n"); continue; }
        if (idx == selected_row) printf("\x1b[7m> %s\x1b[0m\n", labels[idx]);
        else printf("  %s\n", labels[idx]);
    }

    // Status/feedback area under the action row - place it after the visible list
    int status_row = list_top + max_rows + 1;
    if (status_row < list_top + 1) status_row = list_top + 1;
    printf("\x1b[%d;1H", status_row);
    if (g_ui_task_active) {
        // Render task label and progress bar
        char labelbuf[80]; snprintf(labelbuf, sizeof(labelbuf), "%s", g_ui_task_label);
        printf("%s\n", labelbuf);

        // progress bar
        int bar_width = view_cols - 10;
        if (bar_width < 10) bar_width = 10;
        int filled = (g_ui_task_progress * bar_width) / 100;
        printf("[");
        for (int i = 0; i < bar_width; ++i) {
            if (i < filled) putchar('='); else putchar(' ');
        }
        printf("] %d%%\n", g_ui_task_progress);
    } else {
        printf("Status: Idle\n");
    }
    fflush(stdout);

    // Log that render finished and force a console update so the frame
    // is visible on hardware.
    write_ui_log("render_homescreen end: rows=%d cols=%d selected=%d", view_rows, view_cols, selected_row);
    ui_refresh();
}

void ui_refresh(void) {
    consoleUpdate(NULL); // Update console display
}

// Standard menu items (expanded to match homescreen icons/labels)
const char *g_menu_items[] = {
    "File Manager",
    "Game Install/Download",
    "Homebrew Store",
    "Save Manager",
    "System Tools",
    "Settings",
    "Search",
    "Downloads",
    "Logs",
    "Themes",
    "News",
    "Favorites"
};
const int g_menu_count = sizeof(g_menu_items) / sizeof(g_menu_items[0]);

const char *g_settings_lines[] = {
    "Confirm installs: (use Settings menu)",
    "Theme: (use Settings menu)", 
    "Save and return"
};
const int g_settings_count = sizeof(g_settings_lines) / sizeof(g_settings_lines[0]);

const char *g_theme_lines[] = {
    "default",
    "dark",
    "blue"
};
const int g_theme_count = sizeof(g_theme_lines) / sizeof(g_theme_lines[0]);

const char *g_dumps_menu[] = {
    "Save console dump",
    "Save file to dumps",
    "Restore from dumps", 
    "Back"
};
const int g_dumps_count = sizeof(g_dumps_menu) / sizeof(g_dumps_menu[0]);
