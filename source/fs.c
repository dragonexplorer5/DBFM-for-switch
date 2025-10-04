#include "fs.h"
#include <switch.h>
#include "install.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

// helper: return pointer to basename inside provided path buffer
char *local_basename(char *path) {
    char *p = strrchr(path, '/');
    if (!p) p = strrchr(path, '\\');
    return p ? p + 1 : path;
}

int delete_file_at(const char *path) { return remove(path); }

int list_directory(const char *path, char ***out_lines, int *out_count) {
    DIR *d = opendir(path);
    if (!d) return -1;
    struct dirent *ent;
    char **lines = NULL; int count = 0;
    while ((ent = readdir(d)) != NULL) {
        // skip .
        if (strcmp(ent->d_name, ".") == 0) continue;
        // create entry string; append '/' for directories
        char full[1024]; snprintf(full, sizeof(full), "%s%s", path, ent->d_name);
        struct stat st;
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
            // directory: determine if empty and if it contains a .zip-like file
            int is_empty = 1;
            int has_zip = 0;
            DIR *d2 = opendir(full);
            if (d2) {
                struct dirent *e2;
                while ((e2 = readdir(d2)) != NULL) {
                    if (strcmp(e2->d_name, ".") == 0 || strcmp(e2->d_name, "..") == 0) continue;
                    is_empty = 0;
                    // check for .zip extension (case-insensitive)
                    const char *dot = strrchr(e2->d_name, '.');
                    if (dot && (strcasecmp(dot, ".zip") == 0 || strcasecmp(dot, ".zip") == 0)) has_zip = 1;
                    if (!is_empty && !has_zip) break;
                }
                closedir(d2);
            }
            // format: name/ [ZIP] [EMPTY]
            size_t len = strlen(ent->d_name) + 32;
            char *s = malloc(len);
            if (has_zip) snprintf(s, len, "%s/ [ZIP]", ent->d_name);
            else if (is_empty) snprintf(s, len, "%s/ [EMPTY]", ent->d_name);
            else snprintf(s, len, "%s/", ent->d_name);
            lines = realloc(lines, sizeof(char*) * (count + 1)); lines[count++] = s;
        } else {
            char *s = strdup(ent->d_name);
            lines = realloc(lines, sizeof(char*) * (count + 1)); lines[count++] = s;
        }
    }
    closedir(d);
    // add parent entry if not root
    if (strcmp(path, "sdmc:/") != 0) {
        char *p = strdup("../");
        lines = realloc(lines, sizeof(char*) * (count + 1));
        memmove(&lines[1], &lines[0], sizeof(char*) * count);
        lines[0] = p; count++;
    }
    *out_lines = lines; *out_count = count;
    return 0;
}

void prompt_file_action(int view_rows, const char *fullpath, char ***lines_buf, int *total_lines, const char *cur_dir, int *selected_row, int *top_row, int view_cols) {
    // Simple prompt: show options and do local install or delete
    printf("\x1b[%d;1H", view_rows + 2);
    printf("Actions for %s: A=Install (copy), B=Delete, X=Cancel\n", fullpath);
    fflush(stdout);
    // wait for button
    PadState pad; padInitializeDefault(&pad); padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 kd = padGetButtonsDown(&pad);
        if (kd & HidNpadButton_A) {
            // copy to sdmc:/switch/
            install_local_nro(fullpath, view_rows + 3, view_cols);
            break;
        }
        if (kd & HidNpadButton_B) {
            // delete
            remove(fullpath);
            // update list by re-listing
            char **new_lines = NULL; int new_count = 0;
            if (list_directory(cur_dir, &new_lines, &new_count) == 0) {
                // free old
                for (int i = 0; i < *total_lines; ++i) free((*lines_buf)[i]);
                free(*lines_buf);
                *lines_buf = new_lines; *total_lines = new_count; *selected_row = 0; *top_row = 0;
            }
            break;
        }
        if (kd & HidNpadButton_X) break;
        consoleUpdate(NULL);
    }
}

// Helper to ensure dumps directory exists
static void ensure_dumps_dir(void) {
    mkdir("sdmc:/switch/hello-world", 0755);
    mkdir("sdmc:/switch/hello-world/dumps", 0755);
}

int fs_dump_console_text(const char *filename_suffix, const char *text) {
    ensure_dumps_dir();
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char name[512];
    if (filename_suffix && filename_suffix[0]) snprintf(name, sizeof(name), "sdmc:/switch/hello-world/dumps/console-%04d%02d%02d-%02d%02d%02d-%s.txt", tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, filename_suffix);
    else snprintf(name, sizeof(name), "sdmc:/switch/hello-world/dumps/console-%04d%02d%02d-%02d%02d%02d.txt", tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
    FILE *f = fopen(name, "w"); if (!f) return -1;
    fputs(text ? text : "", f);
    fclose(f);
    return 0;
}

int fs_restore_console_text(const char *dump_path) {
    FILE *f = fopen(dump_path, "r"); if (!f) return -1;
    char buf[1024];
    while (fgets(buf, sizeof(buf), f)) {
        printf("%s", buf);
    }
    fclose(f);
    return 0;
}

int fs_dump_file(const char *src_path, const char *dst_name) {
    ensure_dumps_dir();
    char dst[512];
    if (dst_name && dst_name[0]) snprintf(dst, sizeof(dst), "sdmc:/switch/hello-world/dumps/%s", dst_name);
    else {
        // fallback to basename
        const char *p = strrchr(src_path, '/'); if (!p) p = strrchr(src_path, '\\'); const char *base = p ? p+1 : src_path;
        snprintf(dst, sizeof(dst), "sdmc:/switch/hello-world/dumps/%s", base);
    }
    FILE *fs = fopen(src_path, "rb"); if (!fs) return -1;
    FILE *fd = fopen(dst, "wb"); if (!fd) { fclose(fs); return -2; }
    char buf[4096]; size_t r;
    while ((r = fread(buf,1,sizeof(buf),fs)) > 0) fwrite(buf,1,r,fd);
    fclose(fs); fclose(fd);
    return 0;
}

int fs_restore_file(const char *dump_path, const char *dst_target) {
    if (!dump_path || !dst_target) return -1;
    FILE *fs = fopen(dump_path, "rb"); if (!fs) return -1;
    FILE *fd = fopen(dst_target, "wb"); if (!fd) { fclose(fs); return -2; }
    char buf[4096]; size_t r;
    while ((r = fread(buf,1,sizeof(buf),fs)) > 0) fwrite(buf,1,r,fd);
    fclose(fs); fclose(fd);
    return 0;
}
