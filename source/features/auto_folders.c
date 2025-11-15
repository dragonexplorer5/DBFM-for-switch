#include "auto_folders.h"
#include "../fs.h"
#include "../ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <ctype.h>

static void ensure_dir_recursive(const char *path) {
    char tmp[1024];
    strncpy(tmp, path, sizeof(tmp)); tmp[sizeof(tmp)-1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char save = *p; *p = '\0';
            mkdir(tmp, 0755);
            *p = save;
        }
    }
    mkdir(tmp, 0755);
}

static char *str_tolower_dup(const char *s) {
    if (!s) return NULL;
    char *out = strdup(s);
    for (char *p = out; *p; ++p) *p = tolower((unsigned char)*p);
    return out;
}

static int copy_file(const char *src, const char *dst) {
    FILE *fs = fopen(src, "rb"); if (!fs) return -1;
    FILE *fd = fopen(dst, "wb"); if (!fd) { fclose(fs); return -2; }
    char buf[4096]; size_t r;
    while ((r = fread(buf,1,sizeof(buf),fs)) > 0) fwrite(buf,1,r,fd);
    fclose(fs); fclose(fd); return 0;
}

static int move_file(const char *src, const char *dst) {
    if (rename(src, dst) == 0) return 0;
    /* fallback to copy+remove */
    if (copy_file(src, dst) == 0) { remove(src); return 0; }
    return -1;
}

static const char *format_date_from_mtime(time_t mtime) {
    static char buf[32];
    struct tm *tm = localtime(&mtime);
    if (!tm) { strcpy(buf, "unknown"); return buf; }
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday);
    return buf;
}

static void organize_directory(const char *root, int dry_run) {
    char **lines = NULL; int count = 0;
    if (list_directory(root, &lines, &count) != 0) {
        ui_show_error("Auto Folders", "Failed to list directory: %s", root);
        return;
    }

    int moved = 0; int processed = 0;

    for (int i = 0; i < count; ++i) {
        char *entry = lines[i];
        processed++;
        size_t len = strlen(entry);
        int is_dir = 0;
        if (len > 0 && entry[len-1] == '/') is_dir = 1;
        if (is_dir) continue; // only handle files at top level

        char src[1024]; snprintf(src, sizeof(src), "%s%s", root, entry);

        char *lower = str_tolower_dup(entry);
        const char *dest_dir = NULL;
        char dest[1024]; dest[0] = '\0';

        if (strstr(lower, ".nsp") || strstr(lower, ".xci")) {
            // Unused NSPs grouped by guessed game
            char game[128] = "Unknown";
            // guess: take up to first 3 words from filename
            char tmp[256]; strncpy(tmp, entry, sizeof(tmp)); tmp[sizeof(tmp)-1]='\0';
            char *dot = strrchr(tmp, '.'); if (dot) *dot = '\0';
            char *p = tmp; for (int t=0; t<3 && p; ++t) { char *space = strchr(p, ' '); if (space) *space = '\0'; if (t==0) strncpy(game, p, sizeof(game)); if (space) p = space+1; else p = NULL; }
            snprintf(dest, sizeof(dest), "%sUnused NSPs/%s/%s", root, game, entry);
        } else if (strstr(lower, "screenshot") || strstr(lower, ".jpg") || strstr(lower, ".png")) {
            char game[128] = "Unknown";
            char tmp[256]; strncpy(tmp, entry, sizeof(tmp)); tmp[sizeof(tmp)-1]='\0';
            char *dot = strrchr(tmp, '.'); if (dot) *dot = '\0';
            char *p = tmp; for (int t=0; t<3 && p; ++t) { char *space = strchr(p, ' '); if (space) *space = '\0'; if (t==0) strncpy(game, p, sizeof(game)); if (space) p = space+1; else p = NULL; }
            snprintf(dest, sizeof(dest), "%sScreenshots/%s/%s", root, game, entry);
        } else if (strstr(lower, "mod") || strstr(lower, ".zip") || strstr(lower, ".7z")) {
            char game[128] = "Unknown";
            char tmp[256]; strncpy(tmp, entry, sizeof(tmp)); tmp[sizeof(tmp)-1]='\0';
            char *dot = strrchr(tmp, '.'); if (dot) *dot = '\0';
            char *p = tmp; for (int t=0; t<3 && p; ++t) { char *space = strchr(p, ' '); if (space) *space = '\0'; if (t==0) strncpy(game, p, sizeof(game)); if (space) p = space+1; else p = NULL; }
            snprintf(dest, sizeof(dest), "%sMods/%s/%s", root, game, entry);
        } else {
            // default: Downloads by mtime date
            struct stat st;
            if (stat(src, &st) == 0) {
                const char *date = format_date_from_mtime(st.st_mtime);
                snprintf(dest, sizeof(dest), "%sDownloads/%s/%s", root, date, entry);
            } else {
                snprintf(dest, sizeof(dest), "%sDownloads/Unknown/%s", root, entry);
            }
        }

        if (dest[0]) {
            ensure_dir_recursive(dest);
            if (dry_run) {
                printf("[DRY] Move: %s -> %s\n", src, dest);
            } else {
                if (move_file(src, dest) == 0) {
                    moved++;
                } else {
                    printf("Failed to move %s -> %s\n", src, dest);
                }
            }
        }

        free(lower);
    }

    // free lines
    for (int i = 0; i < count; ++i) free(lines[i]); free(lines);

    // show result
    if (dry_run) ui_show_message("Auto Folders", "Dry run complete. Processed %d entries.", processed);
    else ui_show_message("Auto Folders", "Organized %d files.", moved);
}

Result auto_folders_init(void) {
    return 0;
}

void auto_folders_exit(void) {
}

void auto_folders_show_menu(void) {
    MenuItem items[] = {
        {"Scan and Organize (Dry run)", true},
        {"Scan and Organize (Apply)", true},
        {"Back", true}
    };

    while (1) {
        int sel = ui_show_menu("Auto Folders", items, 3);
        if (sel == 0 || sel == 1) {
            char *dir = fs_select_directory("Select directory to scan");
            if (!dir) {
                ui_show_error("Auto Folders", "No directory selected");
                continue;
            }
            int dry = (sel == 0);
            if (!dry) {
                if (!ui_show_dialog("Confirm", "This will move files into new folders. Proceed?")) {
                    free(dir); continue;
                }
            }
            organize_directory(dir, dry);
            free(dir);
        } else {
            return;
        }
    }
}
