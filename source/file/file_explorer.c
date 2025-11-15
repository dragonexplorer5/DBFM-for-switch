#include "file_explorer.h"
#include "../core/input_handler.h"
#include "../include/switch_controls.h"
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

/* forward declaration: sort_directory_listing is defined in file_org.c */
void sort_directory_listing(char** entries, int count, int sort_mode);
#include "functions.h"
#include "graphics.h"
#include "ui.h"
#include "fs.h"
#include "sdcard.h"
#include "../logger.h"
#include "../core/task_queue.h"
#include <errno.h>
#include <switch.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Helper to format file sizes with units
static void format_size(size_t bytes, char* out, size_t out_size) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_idx = 0;
    double size = bytes;
    
    while (size >= 1024.0 && unit_idx < 4) {
        size /= 1024.0;
        unit_idx++;
    }
    
    snprintf(out, out_size, "%.2f %s", size, units[unit_idx]);
}

// Selection-mode globals (file-local)
static bool g_select_mode = false;
static char *g_select_outbuf = NULL;
static size_t g_select_outlen = 0;

// Icon cache to avoid repeated stat calls on visible rows
#define ICON_CACHE_SIZE 64
typedef struct {
    char entry_name[256];
    IconType icon;
} IconCacheEntry;

static IconCacheEntry icon_cache[ICON_CACHE_SIZE];
static int icon_cache_count = 0;

// Lookup icon in cache
static int icon_cache_get(const char* entry_name, IconType* out_icon) {
    for (int i = 0; i < icon_cache_count; ++i) {
        if (strcmp(icon_cache[i].entry_name, entry_name) == 0) {
            *out_icon = icon_cache[i].icon;
            return 1; // cache hit
        }
    }
    return 0; // cache miss
}

// Store icon in cache
static void icon_cache_put(const char* entry_name, IconType icon) {
    // Simple eviction: if cache is full, clear it (could be improved with LRU)
    if (icon_cache_count >= ICON_CACHE_SIZE) {
        icon_cache_count = 0;
    }
    strncpy(icon_cache[icon_cache_count].entry_name, entry_name, sizeof(icon_cache[icon_cache_count].entry_name) - 1);
    icon_cache[icon_cache_count].entry_name[sizeof(icon_cache[icon_cache_count].entry_name) - 1] = '\0';
    icon_cache[icon_cache_count].icon = icon;
    icon_cache_count++;
}

// Clear cache when changing directories
static void icon_cache_clear(void) {
    icon_cache_count = 0;
}

// Minimal file explorer loop that lists a directory and allows navigation.
// This version redraws icons when scrolling/selection changes, keeps selection visible,
// and handles A to descend into folders and B to exit.
Result file_explorer_open(const char *start_dir, int view_rows, int view_cols) {
    // Ensure SD is mounted and healthy before starting the explorer
    Result mrc = sdcard_mount();
    while (R_FAILED(mrc)) {
        log_event(LOG_ERROR, "file_explorer: sdcard mount failed 0x%08x", mrc);
        ui_show_error("SD Card", "Failed to mount SD card (error 0x%08x).\n\nChecklist:\n- Ensure card formatted FAT32/exFAT\n- Reseat card and retry\n- Try a different card\n\nPress Y to retry or B to cancel.", (int)mrc);
        // wait for user retry or cancel
        PadState pad; padInitializeDefault(&pad); padConfigureInput(1, HidNpadStyleSet_NpadStandard);
        while (appletMainLoop()) {
            padUpdate(&pad);
            u64 kd = padGetButtonsDown(&pad);
            if (kd & HidNpadButton_Y) { mrc = sdcard_mount(); break; }
            if (kd & HidNpadButton_B) return 1;
            consoleUpdate(NULL);
        }
    }

    // Basic integrity check
    Result crc = sdcard_check_integrity();
    if (R_FAILED(crc)) {
        log_event(LOG_WARN, "file_explorer: sdcard integrity check failed 0x%08x", crc);
        ui_show_error("SD Card", "SD integrity check failed (0x%08x).\nMake sure card is FAT32/exFAT and seated. Press Y to retry or B to cancel.", (int)crc);
        PadState pad; padInitializeDefault(&pad); padConfigureInput(1, HidNpadStyleSet_NpadStandard);
        while (appletMainLoop()) {
            padUpdate(&pad);
            u64 kd = padGetButtonsDown(&pad);
            if (kd & HidNpadButton_Y) {
                crc = sdcard_check_integrity(); if (R_SUCCEEDED(crc)) break;
            }
            if (kd & HidNpadButton_B) return 1;
            consoleUpdate(NULL);
        }
    }

    char cur_dir[512];
    // Accept '/' as alias for SD root
    const char *initial = start_dir && start_dir[0] ? start_dir : "/";
    char canon_start[PATH_MAX];
    if (sdcard_canonicalize_path(initial, canon_start, sizeof(canon_start)) != 0) {
        strncpy(cur_dir, "sdmc:/", sizeof(cur_dir)-1); cur_dir[sizeof(cur_dir)-1] = '\0';
    } else {
        strncpy(cur_dir, canon_start, sizeof(cur_dir)-1); cur_dir[sizeof(cur_dir)-1] = '\0';
    }

    char **lines_buf = NULL;
    int total_lines = 0;
    // incremental directory loader state
    typedef struct {
        DIR *d;
        char dirpath[PATH_MAX];
        char **lines;
        int count;
        bool done;
    } DirLoader;
    DirLoader loader = {0};
    int selected_row = 0;
    int top_row = 0;
    bool need_redraw = true;

    // Start incremental listing instead of blocking list_directory
    loader.d = NULL; loader.lines = NULL; loader.count = 0; loader.done = false;
    strncpy(loader.dirpath, cur_dir, sizeof(loader.dirpath)-1); loader.dirpath[sizeof(loader.dirpath)-1] = '\0';

    // attempt to open DIR; if opendir fails, fall back immediately
    loader.d = opendir(loader.dirpath);
    if (!loader.d) {
        /* opendir failed; errno captured in logs below */
        log_event(LOG_WARN, "file_explorer: initial opendir('%s') failed: errno=%d", cur_dir, errno);
        ui_show_error("File Explorer", "Cannot open '%s' (error %d). Opening sdmc:/ instead.", cur_dir, errno);
        // attempt sdmc root
        strncpy(cur_dir, "sdmc:/", sizeof(cur_dir)-1); cur_dir[sizeof(cur_dir)-1] = '\0';
        // try opening root
        loader.d = opendir(cur_dir);
        if (!loader.d) {
            ui_show_error("File Explorer", "Failed to open fallback directory sdmc:/. Aborting.");
            return 1;
        }
    }

    graphics_init();
    graphics_load_icons();

    // Initial render (show loading placeholder)
    char *loading = strdup("Loading...");
    lines_buf = malloc(sizeof(char*)); lines_buf[0] = loading; total_lines = 1;
    render_active_view(top_row, selected_row, PAGE_FILE_BROWSER, lines_buf, total_lines, view_rows, view_cols);

    // Draw icons for the initial visible window
    for (int i = 0; i < view_rows && i + top_row < total_lines; ++i) {
        char *entry = lines_buf[top_row + i];
        IconType t = ICON_FILE;
        size_t len = strlen(entry);
        if (len > 0 && entry[len-1] == '/') {
            // folder: check cache first, then stat if not cached
            if (!icon_cache_get(entry, &t)) {
                // cache miss: do the stat and cache result
                char pathbuf[PATH_MAX];
                size_t dirlen = strlen(cur_dir);
                size_t namelen = len;
                if (dirlen + namelen + 1 < sizeof(pathbuf)) {
                    memcpy(pathbuf, cur_dir, dirlen);
                    memcpy(pathbuf + dirlen, entry, namelen);
                    pathbuf[dirlen + namelen] = '\0';
                    if (path_is_zip(pathbuf)) t = ICON_ZIP;
                    else if (directory_is_empty(pathbuf)) t = ICON_EMPTY;
                    else t = ICON_FOLDER;
                } else {
                    t = ICON_FOLDER;
                }
                icon_cache_put(entry, t);
            }
        }
        graphics_draw_icon(i, 1, t);
    }    InputState input_state = {0};
    Result rc = input_handler_init();
    if (R_FAILED(rc)) {
        ui_show_error("Input", "Failed to initialize input handler");
        return rc;
    }

    FileOpsConfig config = {0};
    // Load config or set defaults
    config.enable_rumble = true;
    config.enable_motion = true;

    while (appletMainLoop()) {
    input_handler_update(&input_state);
    // advance background tasks a step each frame so they make progress while UI runs
    task_queue_process();
    log_event(LOG_DEBUG, "file_explorer: update - cur_dir='%s' selected=%d top=%d total=%d", cur_dir, selected_row, top_row, total_lines);
        // process incremental directory loader a few entries per frame to avoid freeze
        if (!loader.done && loader.d) {
            int added = 0;
            struct dirent *ent;
            log_event(LOG_DEBUG, "file_explorer: incremental load - reading entries (current count: %d)", loader.count);
            // if we started with a placeholder, remove it before adding real entries
            if (loader.count == 0 && total_lines == 1 && strcmp(lines_buf[0], "Loading...") == 0) {
                free(lines_buf[0]); free(lines_buf); lines_buf = NULL; total_lines = 0;
                log_event(LOG_DEBUG, "file_explorer: removed Loading placeholder");
            }
                while (added < 40 && (ent = readdir(loader.d)) != NULL) {
                if (strcmp(ent->d_name, ".") == 0) continue;
                if (strcmp(ent->d_name, "..") == 0) continue;
                // append entry name; mark directories with trailing '/'
                char pathbuf[PATH_MAX];
                struct stat st; int is_dir = 0;
                size_t dirlen = strlen(loader.dirpath);
                size_t namelen = strlen(ent->d_name);
                    if (dirlen + namelen + 1 < sizeof(pathbuf)) {
                    /* build path without printf to avoid truncation warnings */
                    memcpy(pathbuf, loader.dirpath, dirlen);
                    memcpy(pathbuf + dirlen, ent->d_name, namelen);
                    pathbuf[dirlen + namelen] = '\0';
                    if (stat(pathbuf, &st) == 0 && S_ISDIR(st.st_mode)) is_dir = 1;
                } else {
                    // path too long to stat safely; assume not a directory to avoid issues
                    is_dir = 0;
                }
                size_t len = strlen(ent->d_name) + (is_dir ? 2 : 1);
                char *s = malloc(len);
                if (is_dir) snprintf(s, len, "%s/", ent->d_name);
                else snprintf(s, len, "%s", ent->d_name);
                loader.lines = realloc(loader.lines, sizeof(char*) * (loader.count + 1));
                loader.lines[loader.count++] = s;
                added++;
            }
            if (!ent) {
                // finished reading
                loader.done = true;
                closedir(loader.d); loader.d = NULL;
                log_event(LOG_INFO, "file_explorer: directory load complete (%d entries)", loader.count);
                // add parent entry if not root
                if (strcmp(loader.dirpath, "sdmc:/") != 0) {
                    char *p = strdup("../");
                    loader.lines = realloc(loader.lines, sizeof(char*) * (loader.count + 1));
                    memmove(&loader.lines[1], &loader.lines[0], sizeof(char*) * loader.count);
                    loader.lines[0] = p; loader.count++;
                }
                // replace lines_buf with loader.lines
                if (lines_buf) { for (int i = 0; i < total_lines; ++i) free(lines_buf[i]); free(lines_buf); }
                lines_buf = loader.lines; total_lines = loader.count; loader.lines = NULL; loader.count = 0;
                need_redraw = true;
            } else {
                // interim additions, merge into lines_buf
                int old = total_lines;
                lines_buf = realloc(lines_buf, sizeof(char*) * (old + added));
                for (int i = 0; i < added; ++i) lines_buf[old + i] = loader.lines[loader.count - added + i];
                total_lines = old + added;
                // trim loader.lines to remove moved entries
                loader.count -= added;
                if (loader.count == 0) { free(loader.lines); loader.lines = NULL; }
                need_redraw = true;
            }
        }
        
        // Process shake gesture for refresh
        if (input_handler_was_shake_detected(&input_state)) {
            // Refresh directory using incremental loader (avoid blocking)
            need_redraw = true;
            // free visible buffer
            if (lines_buf) {
                for (int i = 0; i < total_lines; ++i) free(lines_buf[i]);
                free(lines_buf); lines_buf = NULL; total_lines = 0;
            }
            // reset any existing loader data
            if (loader.d) { closedir(loader.d); loader.d = NULL; }
            if (loader.lines) { for (int i = 0; i < loader.count; ++i) free(loader.lines[i]); free(loader.lines); loader.lines = NULL; loader.count = 0; }
            strncpy(loader.dirpath, cur_dir, sizeof(loader.dirpath)-1); loader.dirpath[sizeof(loader.dirpath)-1] = '\0';
            loader.d = opendir(loader.dirpath);
            if (!loader.d) {
                ui_show_error("Refresh", "Failed to reopen directory");
                // try to fall back to sdmc:/ root
                strncpy(cur_dir, "sdmc:/", sizeof(cur_dir)-1); cur_dir[sizeof(cur_dir)-1] = '\0';
                loader.d = opendir(cur_dir);
                if (!loader.d) {
                    ui_show_error("Refresh", "Failed to reload directory");
                    continue;
                }
            }
            // show loading placeholder until entries are read
            char *loading = strdup("Loading...");
            lines_buf = malloc(sizeof(char*)); lines_buf[0] = loading; total_lines = 1;
            loader.done = false; selected_row = 0; top_row = 0;
            icon_cache_clear(); // clear icon cache on refresh
        }

        // Update sort mode based on tilt
        if (config.enable_motion) {
            static int last_sort_mode = -1;
            int new_sort_mode = input_handler_get_sort_mode(&input_state);
            if (new_sort_mode != last_sort_mode) {
                last_sort_mode = new_sort_mode;
                need_redraw = true;
                // Resort the directory listing based on mode
                sort_directory_listing(lines_buf, total_lines, new_sort_mode);
            }
        }

        // Check for Smart Folders activation
        if (input_handler_check_trigger_combo(&input_state)) {
            // TODO: Implement Smart Folders tab
            ui_show_message("Smart Folders", "Smart Folders feature coming soon!");
            continue;
        }

        // Handle selection changes from input handler
        // input_handler_update uses state->selection_index as an absolute position
        // (keep it in sync here and clamp to valid range to avoid OOB access).
        int prev_selected = selected_row;
        if (total_lines <= 0) {
            selected_row = 0;
        } else {
            if (input_state.selection_index < 0) input_state.selection_index = 0;
            if (input_state.selection_index >= total_lines) input_state.selection_index = total_lines - 1;
            selected_row = input_state.selection_index;
        }

        // Update view based on scroll offset
        top_row = input_state.scroll_offset;
        if (top_row < 0) top_row = 0;
        int max_top = total_lines - view_rows;
        if (max_top < 0) max_top = 0;
        if (top_row > max_top) top_row = max_top;

        // Keep input state selection in sync with selected_row
        input_state.selection_index = selected_row;

        // Selection moved - trigger HD rumble if enabled
        if (config.enable_rumble && prev_selected != selected_row) {
            HidVibrationValue select_value = {
                .freq_low = 150.0f,
                .freq_high = 150.0f,
                .amp_low = 0.1f,
                .amp_high = 0.1f
            };
            input_handler_rumble_feedback(&select_value);
        }

        if (selected_row != prev_selected) {
            need_redraw = true;
        }
        
        // ===== UNIFIED CONTROL SYSTEM: Joy-Con button mapping =====
        // Update control state from Joy-Con input
        SwitchControlState control_state = {0};
        switch_controls_init(&control_state);
        padUpdate(&input_state.pad);
        ControlEvent control = switch_controls_update(&input_state.pad, &control_state);

        // ===== PRIMARY FILE OPERATIONS (ABXY Buttons) =====
        
        // CONTROL_BACK (B) - Go back / close directory
        if (control == CONTROL_BACK) {
            // Exit explorer with rumble
            if (config.enable_rumble) {
                HidVibrationValue exit_value = {
                    .freq_low = 100.0f,
                    .freq_high = 100.0f,
                    .amp_low = 0.3f,
                    .amp_high = 0.3f
                };
                input_handler_rumble_feedback(&exit_value);
            }
            break;
        }
        
        // CONTROL_OPEN (A) - Open selected file/folder or multi-select toggle
        if (control == CONTROL_OPEN) {
            // Activate selected entry with rumble feedback
            if (total_lines <= 0) { /* nothing */ }
            else {
                char *entry = lines_buf[selected_row];
                size_t elen = strlen(entry);
                bool is_folder = (elen > 0 && entry[elen-1] == '/');
                if (is_folder) {
                    // If selection mode is active, choose this folder and exit
                    if (g_select_mode && g_select_outbuf && g_select_outlen > 0) {
                        char selpath[PATH_MAX]; snprintf(selpath, sizeof(selpath), "%s%s", cur_dir, entry);
                        if (sdcard_canonicalize_path(selpath, g_select_outbuf, g_select_outlen) == 0) {
                            // signal selection and exit explorer
                            g_select_mode = false;
                            // copy selected path into cur_dir for logging and break
                            strncpy(cur_dir, g_select_outbuf, sizeof(cur_dir)-1); cur_dir[sizeof(cur_dir)-1] = '\0';
                            // jump to cleanup and return
                            goto explorer_exit;
                        } else {
                            ui_show_error("Selection", "Failed to canonicalize selected path");
                        }
                    }
                    // Attempt to descend into the folder. Keep a copy of previous
                    // state so we can restore on failure rather than returning
                    // and leaking resources or leaving input/graphics in an invalid state.
                    char prev_dir[512]; strncpy(prev_dir, cur_dir, sizeof(prev_dir)-1); prev_dir[sizeof(prev_dir)-1] = '\0';
                    char new_dir[1024];
                    snprintf(new_dir, sizeof(new_dir), "%s%s", cur_dir, entry);
                    // prepare to re-list
                    char **old_lines = lines_buf; int old_total = total_lines;
                    // reset current buffers
                    lines_buf = NULL; total_lines = 0;
                    selected_row = 0; top_row = 0;
                    // copy new_dir into cur_dir (trim if necessary)
                    strncpy(cur_dir, new_dir, sizeof(cur_dir)-1);
                    cur_dir[sizeof(cur_dir)-1] = '\0';
                    // Start incremental re-list using the DirLoader to avoid blocking UI
                    // prepare loader state for new directory
                    // free any existing loader resources
                    if (loader.d) { closedir(loader.d); loader.d = NULL; }
                    if (loader.lines) {
                        for (int i = 0; i < loader.count; ++i) free(loader.lines[i]);
                        free(loader.lines); loader.lines = NULL; loader.count = 0;
                    }

                    // set loader to new directory path
                    strncpy(loader.dirpath, cur_dir, sizeof(loader.dirpath)-1); loader.dirpath[sizeof(loader.dirpath)-1] = '\0';
                    loader.d = opendir(loader.dirpath);
                    if (!loader.d) {
                        // failed to open - restore previous state and notify user
                        ui_show_error("Open Folder", "Failed to open folder: %s", new_dir);
                        log_event(LOG_WARN, "file_explorer: failed to open '%s', restoring '%s'", new_dir, prev_dir);
                        // restore previous listing
                        lines_buf = old_lines; total_lines = old_total;
                        // restore cur_dir
                        strncpy(cur_dir, prev_dir, sizeof(cur_dir)-1); cur_dir[sizeof(cur_dir)-1] = '\0';
                    } else {
                        // success: replace visible buffer with Loading... placeholder until loader fills it
                        if (old_lines) {
                            for (int i = 0; i < old_total; ++i) free(old_lines[i]);
                            free(old_lines);
                        }
                        if (lines_buf) { /* ensure no leak */ }
                        char *loading = strdup("Loading...");
                        lines_buf = malloc(sizeof(char*)); lines_buf[0] = loading; total_lines = 1;
                        selected_row = 0; top_row = 0;
                        loader.done = false; need_redraw = true;
                        icon_cache_clear(); // clear icon cache when changing directories
                    }
                } else {
                    // file selected - call prompt_file_action (defined in fs.c)
                    char fullpath[1024];
                    snprintf(fullpath, sizeof(fullpath), "%s%s", cur_dir, entry);
                    int prev_total = total_lines;
                    // invoke prompt handler which may modify the lines buffer and selection
                    prompt_file_action(view_rows, fullpath, &lines_buf, &total_lines, cur_dir, &selected_row, &top_row, view_cols);
                    log_event(LOG_INFO, "file_explorer: prompt action for '%s' returned; total_lines=%d", fullpath, total_lines);
                    
                    // OPTIMIZATION: If total_lines < 0, caller signaled refresh needed (e.g., after deletion)
                    // Trigger incremental refresh via DirLoader without blocking
                    if (total_lines < 0) {
                        log_event(LOG_INFO, "file_explorer: refresh signal received after file operation");
                        // Free current display buffer
                        if (lines_buf) {
                            for (int i = 0; i < prev_total; ++i) free(lines_buf[i]);
                            free(lines_buf); lines_buf = NULL;
                        }
                        // Clean up any existing loader state
                        if (loader.d) { closedir(loader.d); loader.d = NULL; }
                        if (loader.lines) {
                            for (int i = 0; i < loader.count; ++i) free(loader.lines[i]);
                            free(loader.lines); loader.lines = NULL; loader.count = 0;
                        }
                        // Re-open current directory for incremental reload
                        strncpy(loader.dirpath, cur_dir, sizeof(loader.dirpath)-1);
                        loader.dirpath[sizeof(loader.dirpath)-1] = '\0';
                        loader.d = opendir(loader.dirpath);
                        if (!loader.d) {
                            log_event(LOG_ERROR, "file_explorer: failed to reopen directory for refresh");
                            ui_show_error("Refresh", "Failed to refresh directory");
                        } else {
                            // Show loading placeholder until entries are read
                            char *loading = strdup("Loading...");
                            lines_buf = malloc(sizeof(char*)); lines_buf[0] = loading; total_lines = 1;
                            selected_row = 0; top_row = 0;
                            loader.done = false; loader.count = 0;
                            icon_cache_clear();
                            need_redraw = true;
                        }
                    } else {
                        // Normal operation: redraw after prompt
                        need_redraw = true;
                    }
                }
            }
        }

        if (need_redraw) {
            render_active_view(top_row, selected_row, PAGE_FILE_BROWSER, lines_buf, total_lines, view_rows, view_cols);
            // redraw icons for visible rows (use cache to avoid repeated stat calls)
            for (int i = 0; i < view_rows && i + top_row < total_lines; ++i) {
                char *entry = lines_buf[top_row + i];
                IconType t = ICON_FILE;
                size_t len = strlen(entry);
                if (len > 0 && entry[len-1] == '/') {
                    // folder: check cache first, then stat if not cached
                    if (!icon_cache_get(entry, &t)) {
                        // cache miss: do the stat and cache result
                        char pathbuf[PATH_MAX];
                        size_t dirlen = strlen(cur_dir);
                        size_t namelen = len;
                        if (dirlen + namelen + 1 < sizeof(pathbuf)) {
                            memcpy(pathbuf, cur_dir, dirlen);
                            memcpy(pathbuf + dirlen, entry, namelen);
                            pathbuf[dirlen + namelen] = '\0';
                            if (path_is_zip(pathbuf)) t = ICON_ZIP;
                            else if (directory_is_empty(pathbuf)) t = ICON_EMPTY;
                            else t = ICON_FOLDER;
                        } else {
                            t = ICON_FOLDER;
                        }
                        icon_cache_put(entry, t);
                    }
                }
                graphics_draw_icon(i, 1, t);
            }
            need_redraw = false;
        }

        // Show task progress in status line if any task is running
        Task *cur = task_queue_get_current();
        if (cur) {
            char st[128];
            snprintf(st, sizeof(st), "Task: %d Progress: %d%% %s", cur->type, task_get_progress(cur), cur->status.has_error ? "(error)" : "");
            ui_set_status(st);
        }

        // ===== CONTEXT MENU (Y) - Toggle multi-select mode or show context menu =====
        if (control == CONTROL_CONTEXT_MENU) {
            if (total_lines > 0) {
                char *current = lines_buf[selected_row];
                char is_selected_mark = (current[0] == '*') ? ' ' : '*';
                current[0] = is_selected_mark;
                
                if (config.enable_rumble) {
                    HidVibrationValue select_value = {
                        .freq_low = 180.0f,
                        .freq_high = 180.0f,
                        .amp_low = 0.2f,
                        .amp_high = 0.2f
                    };
                    input_handler_rumble_feedback(&select_value);
                }
                need_redraw = true;
            }
        }
        
        // ===== TAB NAVIGATION (L/R) - Cycle between storage locations =====
        if (control == CONTROL_TAB_PREV || control == CONTROL_TAB_NEXT) {
            // TODO: Implement tab switching (SD card → USB → Internal storage)
            // For now, show placeholder message
            const char* direction = (control == CONTROL_TAB_NEXT) ? "next" : "previous";
            ui_show_message("Storage Tabs", "Switch to %s storage location (coming soon)", direction);
        }
        
        // ===== PAGING (ZL/ZR) - Page up/down in large directories =====
        if (control == CONTROL_PAGE_UP) {
            // Page up: move selection and view up by view_rows
            top_row = (top_row >= view_rows) ? (top_row - view_rows) : 0;
            selected_row = top_row;
            input_state.scroll_offset = top_row;
            input_state.selection_index = selected_row;
            need_redraw = true;
        }
        if (control == CONTROL_PAGE_DOWN) {
            // Page down: move selection and view down by view_rows
            int max_top = total_lines - view_rows;
            if (max_top < 0) max_top = 0;
            top_row = (top_row + view_rows <= max_top) ? (top_row + view_rows) : max_top;
            selected_row = top_row;
            input_state.scroll_offset = top_row;
            input_state.selection_index = selected_row;
            need_redraw = true;
        }
        
        // ===== SEARCH (X) - Toggle search/filter bar =====
        if (control == CONTROL_SEARCH) {
            // TODO: Implement search/filter UI
            ui_show_message("Search", "Search function coming soon!");
        }
        
        // ===== MAIN MENU (+) - File, Edit, View, Tools, Help =====
        if (control == CONTROL_MAIN_MENU) {
            // TODO: Implement main menu
            ui_show_message("Main Menu", "Main menu coming soon!");
        }
        
        // ===== SETTINGS MENU (–) OR TASK CANCEL =====
        if (control == CONTROL_SETTINGS_MENU) {
            // Check if a task is running - if so, cancel it; otherwise show settings
            Task *tcur = task_queue_get_current();
            if (tcur) {
                tcur->cancel = true;
                ui_show_message("Task", "Cancel requested for current task");
            } else {
                // TODO: Implement settings menu
                ui_show_message("Settings", "Settings menu coming soon!");
            }
        }
        
        // ===== SMOOTH SCROLL (Right Stick) =====
        if (control == CONTROL_SCROLL_SMOOTH) {
            int16_t scroll_amount = switch_controls_get_scroll_amount(&control_state);
            if (scroll_amount > 0) {
                // Scroll down
                if (selected_row < total_lines - 1) selected_row++;
                // Update view
                if (selected_row >= top_row + view_rows) {
                    top_row++;
                }
            } else if (scroll_amount < 0) {
                // Scroll up
                if (selected_row > 0) selected_row--;
                // Update view
                if (selected_row < top_row) {
                    top_row--;
                }
            }
            input_state.scroll_offset = top_row;
            input_state.selection_index = selected_row;
            need_redraw = true;
        }
        
        // ===== NAVIGATION REPEATS (D-Pad held) =====
        // Handled by control system via repeat rates; input_handler_update processes held D-Pad
        
        // Bulk operations menu (deprecated; now use Y for context menu and L for bulk ops)
        // Kept for backward compatibility
        u64 kDown = padGetButtonsDown(&input_state.pad);
        if (kDown & HidNpadButton_L) {
            if (total_lines > 0) {
                // gather selected entries (marked by leading '*')
                int selected_count = 0;
                for (int i = 0; i < total_lines; ++i) {
                    if (lines_buf[i] && lines_buf[i][0] == '*') selected_count++;
                }
                if (selected_count == 0) {
                    ui_show_message("Bulk Ops", "No items selected. Use Y to toggle selection.");
                } else {
                    MenuItem items[] = {{"Copy", true}, {"Move", true}, {"Delete", true}, {"Cancel", true}};
                    int choice = ui_show_menu("Bulk Operations", items, 4);
                    if (choice >= 0 && choice <= 2) {
                        char dstbuf[PATH_MAX] = {0};
                        if (choice == 0 || choice == 1) {
                            // ask destination path
                            ui_show_message("Destination", "Enter destination path (sdmc:/ or / for SD root)");
                            if (!ui_show_keyboard("Destination", dstbuf, sizeof(dstbuf))) {
                                ui_show_message("Bulk Ops", "Destination input cancelled.");
                                continue;
                            }
                            // accept '/' alias
                            if (dstbuf[0] == '/' && dstbuf[1] == '\0') strncpy(dstbuf, "sdmc:/", sizeof(dstbuf)-1);
                        }
                        // queue tasks
                        int queued = 0;
                        for (int i = 0; i < total_lines; ++i) {
                            if (!lines_buf[i] || lines_buf[i][0] != '*') continue;
                            const char *name = lines_buf[i] + 1;
                            char src[PATH_MAX]; snprintf(src, sizeof(src), "%s%s", cur_dir, name);
                            if (choice == 2) {
                                // Delete
                                task_queue_add(TASK_DELETE, src, NULL);
                                queued++;
                            } else if (choice == 0 || choice == 1) {
                                // Copy or Move: build dst path
                                char dst[PATH_MAX];
                                if (dstbuf[0] == '\0') continue; // skip
                                // if dst looks like a directory (ends with '/') append name
                                size_t l = strlen(dstbuf);
                                size_t namelen = strlen(name);
                                // guard total length
                                if (l + 1 + namelen + 1 > sizeof(dst)) {
                                    ui_show_message("Bulk Ops", "Destination path too long, skipping %s", name);
                                    continue;
                                }
                                if (l > 0 && dstbuf[l-1] == '/') {
                                    /* build dst without printf to avoid truncation warnings */
                                    memcpy(dst, dstbuf, l);
                                    memcpy(dst + l, name, namelen);
                                    dst[l + namelen] = '\0';
                                } else {
                                    /* need to insert a '/' between dstbuf and name */
                                    memcpy(dst, dstbuf, l);
                                    dst[l] = '/';
                                    memcpy(dst + l + 1, name, namelen);
                                    dst[l + 1 + namelen] = '\0';
                                }
                                task_queue_add(choice == 0 ? TASK_COPY : TASK_MOVE, src, dst);
                                queued++;
                            }
                        }
                        ui_show_message("Bulk Ops", "Queued %d tasks.", queued);
                        // optionally show a simple task progress modal
                        ui_show_message("Bulk Ops", "Tasks queued. They will be processed in background.");
                    }
                }
            }
            need_redraw = true;
        }
        
        // Handle properties view with X
        if (kDown & HidNpadButton_X) {
            if (total_lines > 0) {
                char *entry = lines_buf[selected_row];
                char full_path[1024];
                snprintf(full_path, sizeof(full_path), "%s%s", cur_dir, entry);
                
                struct stat st;
                if (stat(full_path, &st) == 0) {
                    char size_str[32];
                    format_size(st.st_size, size_str, sizeof(size_str));

                    char date_str[64];
                    strftime(date_str, sizeof(date_str), "%Y-%m-%d %H:%M:%S", localtime(&st.st_mtime));

                    // If it's a large file/directory, add haptic feedback
                    if (config.enable_rumble && st.st_size > 1024*1024*100) { // 100MB
                        HidVibrationValue large_file_value = {
                            .freq_low = 40.0f,
                            .freq_high = 100.0f,
                            .amp_low = 0.8f,
                            .amp_high = 0.4f
                        };
                        input_handler_rumble_feedback(&large_file_value);
                    }

                    char msg[512];
                    snprintf(msg, sizeof(msg),
                            "Name: %s\nSize: %s\nModified: %s\nPermissions: %o\nType: %s",
                            entry,
                            size_str,
                            date_str,
                            st.st_mode & 0777,
                            S_ISDIR(st.st_mode) ? "Directory" : "File");

                    ui_show_message("Properties", msg);
                } else {
                    log_event(LOG_WARN, "file_explorer: stat failed for '%s' (errno=%d)", full_path, errno);
                    ui_show_error("Properties", "Failed to stat '%s' (errno=%d)", full_path, errno);
                }
            }
        }

        consoleUpdate(NULL);
    }

    // cleanup
explorer_exit:
    for (int i = 0; i < total_lines; ++i) free(lines_buf[i]);
    free(lines_buf);
    
    // Clean up input handler and graphics
    input_handler_exit();
    graphics_shutdown();
    
    return 0;
}

// A placeholder prompt_file_action that simply prints the selected path and returns.
// prompt_file_action is provided by fs.c (prototype in fs.h)

int file_explorer_select_directory(const char* start_dir, char* out_buf, size_t out_len, int view_rows, int view_cols) {
    if (!out_buf || out_len == 0) return -1;
    // set selection mode globals
    g_select_mode = true;
    g_select_outbuf = out_buf;
    g_select_outlen = out_len;
    // call main explorer; it will set g_select_outbuf on selection and goto explorer_exit
    int rc = file_explorer_open(start_dir, view_rows, view_cols);
    // if g_select_outbuf was filled, rc should be 0; clear selection globals
    g_select_mode = false;
    g_select_outbuf = NULL;
    g_select_outlen = 0;
    // rc==0 indicates explorer exit; check whether out_buf contains a path
    if (out_buf[0] == '\0') return -1;
    return rc;
}
