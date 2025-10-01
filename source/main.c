#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>
#include <switch.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ctype.h>

// Default grid dimensions (used as fallback)
#define DEFAULT_GRID_ROWS 24
#define DEFAULT_GRID_COLS 40

// Cell width: number of characters per grid column (use 2 for wider cells)
#define DEFAULT_CELL_W 2
// Blink interval in milliseconds
#define DEFAULT_BLINK_MS 400

static int cell_w = DEFAULT_CELL_W;
static int blink_ms = DEFAULT_BLINK_MS;

#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <libgen.h>
#include <stdarg.h>
#include "settings.h"
#include "install.h"
#include "fs.h"
#include "ui.h"

// Draw a GRID_ROWS x GRID_COLS grid and highlight the given row/column using ANSI colors.
// Try to query the terminal for its size by asking for cursor position after moving to a large coordinate.
// Writes ESC[999;999H ESC[6n and expects ESC[row;colR] on stdin. Returns true on success.
// Query terminal size using ANSI cursor-report. Returns true on success.
static bool get_terminal_size(int *out_rows, int *out_cols) {
    // Move to a large coordinate then request cursor position
    printf("\x1b[999;999H");
    printf("\x1b[6n");
    fflush(stdout);

    char buf[64];
    int idx = 0;
    struct timeval tv = {0, 200000}; // 200ms total timeout
    int fd = fileno(stdin);
    fd_set rfds;

    while (idx < (int)sizeof(buf) - 1) {
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        if (select(fd + 1, &rfds, NULL, NULL, &tv) <= 0) break;
        int ch = getchar();
        if (ch == EOF) break;
        buf[idx++] = (char)ch;
        if (ch == 'R') break; // end of response
        tv.tv_sec = 0; tv.tv_usec = 100000; // shorten subsequent waits
    }
    buf[idx] = '\0';

    if (idx <= 0) return false;
    char *p = strstr(buf, "\x1b[");
    if (!p) return false;
    int r = 0, c = 0;
    if (sscanf(p, "\x1b[%d;%dR", &r, &c) != 2) return false;
    if (r <= 0 || c <= 0) return false;
    *out_rows = r; *out_cols = c;
    return true;
}

// UI rendering functions moved to ui.c/ui.h

// Application pages are defined in ui.h

// Interactive settings menu: toggle confirm_installs and cycle themes, persist on exit
static void settings_menu(int view_rows, int view_cols) {
    int sel = 0; // Confirm, Theme, Save & Back
    char buf[128];
    PadState p; padInitializeDefault(&p); padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    while (appletMainLoop()) {
        // build display lines
        const char *lines[16]; int n = 0;
    snprintf(buf, sizeof(buf), "Confirm installs: %s", g_settings.confirm_installs ? "On" : "Off"); lines[n++] = strdup(buf);
    snprintf(buf, sizeof(buf), "Theme: %s", g_settings.theme); lines[n++] = strdup(buf);
        lines[n++] = "Save and return";
        render_text_view(0, sel, lines, n, view_rows, view_cols);
        // free temporary duped lines except the static one
        free((void*)lines[0]); free((void*)lines[1]);

        padUpdate(&p);
        u64 kd = padGetButtonsDown(&p);
        if (kd & HidNpadButton_Down) { sel = (sel + 1) % n; }
        if (kd & HidNpadButton_Up) { sel = (sel - 1 + n) % n; }
        if (kd & HidNpadButton_A) {
            if (sel == 0) { g_settings.confirm_installs = !g_settings.confirm_installs; save_settings(); }
            else if (sel == 1) {
                // cycle themes
                const char *themes[] = { "default", "dark", "blue" };
                int idx = 0; for (int i = 0; i < (int)(sizeof(themes)/sizeof(themes[0])); ++i) if (strcmp(g_settings.theme, themes[i]) == 0) { idx = i; break; }
                idx = (idx + 1) % (int)(sizeof(themes)/sizeof(themes[0])); strncpy(g_settings.theme, themes[idx], sizeof(g_settings.theme)-1); g_settings.theme[sizeof(g_settings.theme)-1] = '\0'; apply_theme(g_settings.theme); save_settings();
            } else if (sel == 2) { break; }
        }
        if (kd & HidNpadButton_B) { break; }
        consoleUpdate(NULL);
    }
    // on exit, ensure settings persisted
    save_settings();
}

// (AppSettings and g_settings are declared earlier near prototypes)

// settings are implemented in settings.c

// Theme handling centralized in settings.c (accessors available via settings.h)

// menu/theme arrays moved to ui_data.c

// Filesystem helpers moved to fs.c (see fs.h)

// ... old grid helpers removed (clamp_cursor, handle_input) — replaced by text view navigation.

static void update_blink(bool *blink, struct timespec *last, int interval_ms) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long ms = (now.tv_sec - last->tv_sec) * 1000 + (now.tv_nsec - last->tv_nsec) / 1000000;
    if (ms >= interval_ms) { *blink = !*blink; *last = now; }
}

// scan_installs implemented in install.c

// UI rendering is implemented in ui.c (render_active_view / render_text_view)

static int prompt_confirm(int gr, const char *msg) {
    int row = gr + 2 + g_candidate_count + 1;
    printf("\x1b[%d;1H", row);
    printf("%s (A=Yes B=No)\n", msg);
    fflush(stdout);
    // Wait for a button press
    PadState pad; padInitializeDefault(&pad); padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 kd = padGetButtonsDown(&pad);
        if (kd & HidNpadButton_A) return 1;
        if (kd & HidNpadButton_B) return 0;
        consoleUpdate(NULL);
    }
    return 0;
}
// Downloader and staged install moved to install.c

int main(int argc, char **argv)
{
    // CLI: allow specifying generated line count via --lines N
    int gen_lines = 0;
    // Simple CLI: allow overriding cell width and blink interval
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--cell-w") == 0 && i + 1 < argc) {
            cell_w = atoi(argv[++i]);
            if (cell_w < 1) cell_w = DEFAULT_CELL_W;
        } else if (strcmp(argv[i], "--lines") == 0 && i + 1 < argc) {
            gen_lines = atoi(argv[++i]);
            if (gen_lines < 0) gen_lines = 0;
        } else if (strcmp(argv[i], "--blink-ms") == 0 && i + 1 < argc) {
            blink_ms = atoi(argv[++i]);
            if (blink_ms < 50) blink_ms = DEFAULT_BLINK_MS; // guard against too small
        }
    }

    


    u64 LanguageCode=0;
    SetLanguage Language=SetLanguage_ENUS;

    consoleInit(NULL);

    // Configure our supported input layout: a single player with standard controller styles
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    // Initialize the default gamepad (which reads handheld mode inputs as well as the first connected controller)
    PadState pad;
    padInitializeDefault(&pad);

    Result rc = setInitialize();
    if (R_FAILED(rc)) printf("setInitialize() failed: 0x%x.\n", rc);

    if (R_SUCCEEDED(rc))
    {
        //Get system language.
        rc = setGetSystemLanguage(&LanguageCode);
        if (R_FAILED(rc)) printf("setGetSystemLanguage() failed: 0x%x.\n", rc);
    }

    if (R_SUCCEEDED(rc))
    {
        printf("setGetSystemLanguage() LanguageCode: %s\n", (char*)&LanguageCode);

        // Convert LanguageCode to Language. Use this if you need IDs, not strings.
        rc = setMakeLanguage(LanguageCode, &Language);

        if (R_FAILED(rc)) printf("setMakeLanguage() failed: 0x%x.\n", rc);
    }

    if (R_SUCCEEDED(rc))
    {
        printf("Language: %d\n", Language);

        // Converts the input Language to LanguageCode.
        rc = setMakeLanguageCode(SetLanguage_JA, &LanguageCode);

        if (R_FAILED(rc)) printf("setMakeLanguageCode() failed: 0x%x.\n", rc);
    }

    if (R_SUCCEEDED(rc)) printf("New LanguageCode: %s\n", (char*)&LanguageCode);

    //Initialize console. Using NULL as the second argument tells the console library to use the internal console structure as current one.
    consoleInit(NULL);

    // Configure our supported input layout: a single player with standard controller styles
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    

    // load persisted settings and apply theme
    load_settings();
    apply_theme(g_settings.theme);

    // determine terminal size for text view (fallback to defaults)
    int term_r = 0, term_c = 0;
    int view_rows = DEFAULT_GRID_ROWS, view_cols = 80;
    if (get_terminal_size(&term_r, &term_c)) {
        if (term_r > 2) view_rows = term_r - 1; // reserve status line
        view_cols = term_c;
    }

    // Text buffer and selection state (we'll populate `lines_buf` dynamically)
    char **lines_buf = NULL; int total_lines = 0;
    int selected_row = 0;
    int top_row = 0;
    // current directory for file browser (always keep trailing slash)
    char cur_dir[512];
    strncpy(cur_dir, "sdmc:/", sizeof(cur_dir)-1); cur_dir[sizeof(cur_dir)-1] = '\0';

    // If gen_lines specified, generate numbered lines. If not, default to view_rows * 5
    if (gen_lines == 0) gen_lines = view_rows * 5;
    lines_buf = malloc(sizeof(char*) * gen_lines);
    for (int i = 0; i < gen_lines; ++i) {
        int need = 64;
        char *s = malloc(need);
        snprintf(s, need, "Line %d: generated content", i+1);
        lines_buf[i] = s;
    }
    total_lines = gen_lines;

    bool blink = true; struct timespec last; clock_gettime(CLOCK_MONOTONIC, &last);
    // main menu is provided by g_menu_items/g_menu_count
    AppPage page = PAGE_MAIN_MENU;
    // show main menu initially
    render_active_view(top_row, selected_row, page, lines_buf, total_lines, view_rows, view_cols);

    // initial scan of installs
    scan_installs(g_candidates, g_candidate_count);
    int list_mode = 0; // 0 = grid navigation, 1 = list navigation
    int list_persistent = 0; // when set, the list stays up until B is pressed (opened via double-A)
    int selected_idx = 0;
    static struct timespec last_a = {0,0};
    while (appletMainLoop()) {
        // Scan the gamepad once per frame
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);

        // Toggle list navigation with Y (unless the list was opened persistently via A double-press)
            if (kDown & HidNpadButton_Y) {
                if (!list_persistent) {
                list_mode = !list_mode;
                if (list_mode) show_install_list(view_rows, g_candidates, g_candidate_count, selected_idx);
                else render_active_view(top_row, selected_row, page, lines_buf, total_lines, view_rows, view_cols);
            } else {
                    // If persistent, just re-show the list so it stays visible
                    show_install_list(view_rows, g_candidates, g_candidate_count, selected_idx);
                }
            }

        // A handling: single press prints, double-press confirms and installs
        if (kDown & HidNpadButton_A) {
            struct timespec now_a; clock_gettime(CLOCK_MONOTONIC, &now_a);
            long ms = (now_a.tv_sec - last_a.tv_sec) * 1000 + (now_a.tv_nsec - last_a.tv_nsec) / 1000000;
            if (ms > 0 && ms <= 400) {
                int idx = list_mode ? selected_idx : selected_row;
                if (idx < 0) idx = 0;
                if (idx >= g_candidate_count) idx = g_candidate_count - 1;
                // Open the install list persistently so it stays up until B is pressed.
                list_mode = 1; list_persistent = 1; selected_idx = idx;
                show_install_list(view_rows, g_candidates, g_candidate_count, selected_idx);
                char msg[256]; snprintf(msg, sizeof(msg), "Install %s?", g_candidates[selected_idx].name);
                int confirm = prompt_confirm(view_rows, msg);
                if (confirm) {
                    int progress_row = view_rows + 2 + g_candidate_count + 2;
                    printf("\x1b[%d;1H", progress_row);
                    printf("Starting staged install: %s\n", g_candidates[selected_idx].name);
                    fflush(stdout);
                    int res = staged_install(g_candidates[selected_idx].name, g_candidates[selected_idx].url, progress_row, view_cols);
                    if (res == 0) printf("\x1b[%d;1HInstall complete: %s\x1b[K\n", progress_row, g_candidates[selected_idx].name);
                    else if (res == -99) printf("\x1b[%d;1HInstall canceled by user\x1b[K\n", progress_row);
                    else printf("\x1b[%d;1HInstall failed (%d)\x1b[K\n", progress_row, res);
                    scan_installs(g_candidates, g_candidate_count);
                    show_install_list(view_rows, g_candidates, g_candidate_count, selected_idx);
                }
            } else {
                // single A press: behavior depends on current page
                if (page == PAGE_MAIN_MENU) {
                    if (selected_row == 0) {
                        char **dir_lines = NULL; int dir_count = 0;
                        if (list_directory(cur_dir, &dir_lines, &dir_count) == 0 && dir_count > 0) {
                            for (int i = 0; i < total_lines; ++i) free(lines_buf[i]);
                            free(lines_buf);
                            lines_buf = dir_lines; total_lines = dir_count; selected_row = 0; top_row = 0; page = PAGE_FILE_BROWSER;
                            render_active_view(top_row, selected_row, page, lines_buf, total_lines, view_rows, view_cols);
                        }
                    } else if (selected_row == 1) {
                        page = PAGE_DOWNLOADS; list_mode = 1; list_persistent = 0; selected_idx = 0;
                        show_install_list(view_rows, g_candidates, g_candidate_count, selected_idx);
                    } else if (selected_row == 2) {
                        // Dumps submenu
                        int dsel = 0;
                        while (appletMainLoop()) {
                            render_text_view(0, dsel, g_dumps_menu, g_dumps_count, view_rows, view_cols);
                            padUpdate(&pad);
                            u64 kd2 = padGetButtonsDown(&pad);
                            if (kd2 & HidNpadButton_Down) dsel = (dsel + 1) % g_dumps_count;
                            if (kd2 & HidNpadButton_Up) dsel = (dsel - 1 + g_dumps_count) % g_dumps_count;
                            if (kd2 & HidNpadButton_A) {
                                if (dsel == 0) {
                                    // Save console dump
                                    fs_dump_console_text(NULL, "Console dump saved by user\n");
                                } else if (dsel == 1) {
                                    // Save file to dumps: use current file selection if in file browser
                                    if (page == PAGE_FILE_BROWSER && total_lines > 0) {
                                        char selpath[1024]; snprintf(selpath, sizeof(selpath), "%s%s", cur_dir, lines_buf[selected_row]);
                                        fs_dump_file(selpath, NULL);
                                    } else {
                                        char sample[256] = "sdmc:/switch/hello-world/settings.cfg";
                                        fs_dump_file(sample, NULL);
                                    }
                                } else if (dsel == 2) {
                                    // Restore from dumps: let user pick a dump and confirm target
                                    char **dump_list = NULL; int dump_count = 0;
                                    if (list_directory("sdmc:/switch/hello-world/dumps/", &dump_list, &dump_count) == 0 && dump_count > 0) {
                                        int dpick = 0;
                                        while (appletMainLoop()) {
                                            render_text_view(0, dpick, (const char **)dump_list, dump_count, view_rows, view_cols);
                                            padUpdate(&pad);
                                            u64 kd3 = padGetButtonsDown(&pad);
                                            if (kd3 & HidNpadButton_Down) dpick = (dpick + 1) % dump_count;
                                            if (kd3 & HidNpadButton_Up) dpick = (dpick - 1 + dump_count) % dump_count;
                                            if (kd3 & HidNpadButton_A) {
                                                char path[512]; snprintf(path, sizeof(path), "sdmc:/switch/hello-world/dumps/%s", dump_list[dpick]);
                                                // Confirm restore
                                                char confirm_msg[256]; snprintf(confirm_msg, sizeof(confirm_msg), "Restore %s?", dump_list[dpick]);
                                                if (prompt_confirm(view_rows, confirm_msg)) {
                                                    size_t L = strlen(path);
                                                    if (L > 4 && strcmp(path + L - 4, ".txt") == 0) {
                                                        fs_restore_console_text(path);
                                                    } else {
                                                        // ask for overwrite confirmation for target
                                                        char dest[512]; snprintf(dest, sizeof(dest), "sdmc:/switch/hello-world/restored_%s", dump_list[dpick]);
                                                        if (prompt_confirm(view_rows, "Overwrite target if exists?")) {
                                                            fs_restore_file(path, dest);
                                                        }
                                                    }
                                                }
                                                break;
                                            }
                                            if (kd3 & HidNpadButton_B) break;
                                            consoleUpdate(NULL);
                                        }
                                        for (int i = 0; i < dump_count; ++i) free(dump_list[i]);
                                        free(dump_list);
                                    }
                                } else if (dsel == 3) {
                                    break; // back
                                }
                            }
                            if (kd2 & HidNpadButton_B) break;
                            consoleUpdate(NULL);
                        }
                    } else if (selected_row == 2) {
                        page = PAGE_SETTINGS; settings_menu(view_rows, view_cols);
                        page = PAGE_MAIN_MENU;
                        render_active_view(top_row, 0, page, lines_buf, total_lines, view_rows, view_cols);
                    } else if (selected_row == 3) {
                        page = PAGE_THEMES; render_active_view(top_row, 0, page, lines_buf, total_lines, view_rows, view_cols);
                    } else if (selected_row == 4) {
                        break; // exit
                    }
                } else if (page == PAGE_FILE_BROWSER) {
                    if (total_lines > 0) {
                        char *entry = lines_buf[selected_row];
                        int elen = strlen(entry);
                        if (strcmp(entry, "..") == 0 || strcmp(entry, "../") == 0) {
                            int l = strlen(cur_dir);
                            if (l > 1 && cur_dir[l-1] == '/') { cur_dir[l-1] = '\0'; l--; }
                            char *p = strrchr(cur_dir, '/');
                            if (p && p > cur_dir + 5) { int newlen = (int)(p - cur_dir) + 1; cur_dir[newlen] = '\0'; }
                            else { strncpy(cur_dir, "sdmc:/", sizeof(cur_dir)-1); cur_dir[sizeof(cur_dir)-1] = '\0'; }
                            char **dir_lines = NULL; int dir_count = 0;
                            if (list_directory(cur_dir, &dir_lines, &dir_count) == 0) {
                                for (int i = 0; i < total_lines; ++i) free(lines_buf[i]);
                                free(lines_buf);
                                lines_buf = dir_lines; total_lines = dir_count; selected_row = 0; top_row = 0;
                                render_text_view(top_row, selected_row, (const char **)lines_buf, total_lines, view_rows, view_cols);
                            }
                        } else if (elen > 0 && entry[elen-1] == '/') {
                            char newdir[512]; size_t cur_len = strlen(cur_dir); size_t entry_len = strlen(entry);
                            if (cur_len + entry_len + 2 > sizeof(newdir)) {
                                int avail = (int)(sizeof(newdir) - cur_len - 2); if (avail < 0) avail = 0;
                                strncpy(newdir, cur_dir, sizeof(newdir)-1); newdir[sizeof(newdir)-1] = '\0'; strncat(newdir, entry, avail);
                            } else { strcpy(newdir, cur_dir); strcat(newdir, entry); }
                            if (newdir[strlen(newdir)-1] != '/') strncat(newdir, "/", sizeof(newdir)-strlen(newdir)-1);
                            char **dir_lines = NULL; int dir_count = 0;
                            if (list_directory(newdir, &dir_lines, &dir_count) == 0) {
                                strncpy(cur_dir, newdir, sizeof(cur_dir)-1); cur_dir[sizeof(cur_dir)-1] = '\0';
                                for (int i = 0; i < total_lines; ++i) {
                                    free(lines_buf[i]);
                                }
                                free(lines_buf);
                                lines_buf = dir_lines; total_lines = dir_count; selected_row = 0; top_row = 0;
                                render_text_view(top_row, selected_row, (const char **)lines_buf, total_lines, view_rows, view_cols);
                            } else { printf("\x1b[%d;1HCannot enter %s\x1b[K\n", view_rows + 2, entry); fflush(stdout); }
                        } else {
                            char fullpath[1024]; snprintf(fullpath, sizeof(fullpath), "%s%s", cur_dir, entry);
                            prompt_file_action(view_rows, fullpath, &lines_buf, &total_lines, cur_dir, &selected_row, &top_row, view_cols);
                            render_active_view(top_row, selected_row, page, lines_buf, total_lines, view_rows, view_cols);
                        }
                    }
                } else if (page == PAGE_TEXT_VIEW) {
                    printf("\x1b[%d;1H", view_rows + 2);
                    printf("Selected line: %d - %s\x1b[K\n", selected_row + 1, lines_buf[selected_row]); fflush(stdout);
                }
            }
            last_a = now_a;
        }

        // Navigation: either list selection or grid movement
        if (list_mode) {
            if (kDown & HidNpadButton_Down) { selected_idx = (selected_idx + 1) % g_candidate_count; show_install_list(view_rows, g_candidates, g_candidate_count, selected_idx); }
            if (kDown & HidNpadButton_Up) { selected_idx = (selected_idx - 1 + g_candidate_count) % g_candidate_count; show_install_list(view_rows, g_candidates, g_candidate_count, selected_idx); }
        } else {
                // move selection one row at a time and scroll as needed
            if (kDown & HidNpadButton_Down) {
                if (selected_row < total_lines - 1) selected_row++;
                // if selection moved past bottom of view, scroll down
                if (selected_row >= top_row + view_rows) top_row = selected_row - view_rows + 1;
                render_active_view(top_row, selected_row, page, lines_buf, total_lines, view_rows, view_cols);
            }
            if (kDown & HidNpadButton_Up) {
                if (selected_row > 0) selected_row--;
                if (selected_row < top_row) top_row = selected_row;
                render_active_view(top_row, selected_row, page, lines_buf, total_lines, view_rows, view_cols);
            }
            // PageUp/PageDown: map to L/R shoulder buttons for faster scrolling
            if (kDown & HidNpadButton_L) {
                selected_row -= view_rows; if (selected_row < 0) selected_row = 0;
                if (selected_row < top_row) top_row = selected_row;
                render_active_view(top_row, selected_row, page, lines_buf, total_lines, view_rows, view_cols);
            }
            if (kDown & HidNpadButton_R) {
                selected_row += view_rows; if (selected_row >= total_lines) selected_row = total_lines - 1;
                if (selected_row >= top_row + view_rows) top_row = selected_row - view_rows + 1;
                render_active_view(top_row, selected_row, page, lines_buf, total_lines, view_rows, view_cols);
            }
        }

        // simple blink update kept for compatibility but not used in text view
        update_blink(&blink, &last, blink_ms);

        // If B is pressed while the list is persistent, dismiss the list and return to text view.
        if (kDown & HidNpadButton_B) {
            if (list_persistent) {
                list_persistent = 0;
                list_mode = 0;
                // redraw current active view
                render_active_view(top_row, selected_row, page, lines_buf, total_lines, view_rows, view_cols);
            } else {
                break; // normal behaviour: exit to hbmenu
            }
        }
        consoleUpdate(NULL);
    }
    
    setExit();
    consoleExit(NULL);
    return 0;
}
