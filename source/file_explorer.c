#include "file_explorer.h"
#include "functions.h"
#include "graphics.h"
#include "ui.h"
#include "fs.h"
#include <switch.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Minimal file explorer loop that lists a directory and allows navigation.
// This version redraws icons when scrolling/selection changes, keeps selection visible,
// and handles A to descend into folders and B to exit.
int file_explorer_open(const char *start_dir, int view_rows, int view_cols) {
    char cur_dir[512];
    strncpy(cur_dir, start_dir, sizeof(cur_dir)-1);
    cur_dir[sizeof(cur_dir)-1] = '\0';

    char **lines_buf = NULL;
    int total_lines = 0;
    int selected_row = 0;
    int top_row = 0;
    bool need_redraw = true;

    if (list_directory(cur_dir, &lines_buf, &total_lines) != 0) return -1;

    graphics_init();
    graphics_load_icons();

    // Initial render
    render_active_view(top_row, selected_row, PAGE_FILE_BROWSER, lines_buf, total_lines, view_rows, view_cols);

    // Draw icons for the initial visible window
    for (int i = 0; i < view_rows && i + top_row < total_lines; ++i) {
        char *entry = lines_buf[top_row + i];
        IconType t = ICON_FILE;
        size_t len = strlen(entry);
        if (len > 0 && entry[len-1] == '/') {
            // folder: check for zip or empty
            char pathbuf[1024];
            snprintf(pathbuf, sizeof(pathbuf), "%s%s", cur_dir, entry);
            if (path_is_zip(pathbuf)) t = ICON_ZIP;
            else if (directory_is_empty(pathbuf)) t = ICON_EMPTY;
            else t = ICON_FOLDER;
        }
        graphics_draw_icon(i, 1, t);
    }

    PadState pad;
    padInitializeDefault(&pad);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 kd = padGetButtonsDown(&pad);
        u64 kh = padGetButtonsHeld(&pad);

        if (kd & HidNpadButton_B) {
            // exit explorer
            break;
        }

        if (kd & HidNpadButton_Up) {
            if (selected_row > 0) {
                selected_row--;
                if (selected_row < top_row) top_row = selected_row;
                need_redraw = true;
            }
        } else if (kd & HidNpadButton_Down) {
            if (selected_row < total_lines - 1) {
                selected_row++;
                if (selected_row >= top_row + view_rows) top_row = selected_row - view_rows + 1;
                need_redraw = true;
            }
        } else if (kd & HidNpadButton_A) {
            // Activate selected entry
            if (total_lines <= 0) { /* nothing */ }
            else {
                char *entry = lines_buf[selected_row];
                size_t elen = strlen(entry);
                bool is_folder = (elen > 0 && entry[elen-1] == '/');
                if (is_folder) {
                    // build new dir path
                    char new_dir[1024];
                    snprintf(new_dir, sizeof(new_dir), "%s%s", cur_dir, entry);
                    // ensure trailing slash present for list_directory usage
                    // reload directory
                    for (int i = 0; i < total_lines; ++i) free(lines_buf[i]);
                    free(lines_buf);
                    lines_buf = NULL;
                    total_lines = 0;
                    selected_row = 0;
                    top_row = 0;
                    // copy new_dir into cur_dir (trim if necessary)
                    strncpy(cur_dir, new_dir, sizeof(cur_dir)-1);
                    cur_dir[sizeof(cur_dir)-1] = '\0';
                    // re-list
                    if (list_directory(cur_dir, &lines_buf, &total_lines) != 0) {
                        // failed to open - try to restore previous dir (not handled here)
                        return -1;
                    }
                    need_redraw = true;
                } else {
                    // file selected - call prompt_file_action if available (defined in fs.c)
                    // The prototype is in fs.h: prompt_file_action(const char *full_path)
                    char fullpath[1024];
                    snprintf(fullpath, sizeof(fullpath), "%s%s", cur_dir, entry);
                    // trim trailing newline or whitespace
                    prompt_file_action(fullpath);
                    // after action, we will redraw to ensure UI remains consistent
                    need_redraw = true;
                }
            }
        }

        if (need_redraw) {
            render_active_view(top_row, selected_row, PAGE_FILE_BROWSER, lines_buf, total_lines, view_rows, view_cols);
            // redraw icons for visible rows
            for (int i = 0; i < view_rows && i + top_row < total_lines; ++i) {
                char *entry = lines_buf[top_row + i];
                IconType t = ICON_FILE;
                size_t len = strlen(entry);
                if (len > 0 && entry[len-1] == '/') {
                    char pathbuf[1024];
                    snprintf(pathbuf, sizeof(pathbuf), "%s%s", cur_dir, entry);
                    if (path_is_zip(pathbuf)) t = ICON_ZIP;
                    else if (directory_is_empty(pathbuf)) t = ICON_EMPTY;
                    else t = ICON_FOLDER;
                }
                graphics_draw_icon(i, 1, t);
            }
            need_redraw = false;
        }

        consoleUpdate(NULL);
    }

    // cleanup
    for (int i = 0; i < total_lines; ++i) free(lines_buf[i]);
    free(lines_buf);
    graphics_shutdown();
    return 0;
}

// A placeholder prompt_file_action that simply prints the selected path and returns.
// prompt_file_action is provided by fs.c (prototype in fs.h)
