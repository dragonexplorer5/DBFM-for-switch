#include "fs.h"
#include <switch.h>
#include "install.h"
#include "sdcard.h"
#include "../logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

// helper: return pointer to basename inside provided path buffer
char *local_basename(char *path) {
    char *p = strrchr(path, '/');
    if (!p) p = strrchr(path, '\\');
    return p ? p + 1 : path;
}

int delete_file_at(const char *path) { return remove(path); }

int list_directory(const char *path, char ***out_lines, int *out_count) {
    // Only allow SD paths (canonicalize)
    char canon[PATH_MAX];
    if (sdcard_canonicalize_path(path, canon, sizeof(canon)) != 0) {
        log_event(LOG_WARN, "fs: list_directory rejected non-sd path '%s'", path);
        return -EINVAL;
    }

    DIR *d = opendir(canon);
    if (!d) {
        log_event(LOG_WARN, "fs: opendir('%s') failed errno=%d", canon, errno);
        return -errno;
    }
    struct dirent *ent;
    char **lines = NULL; int count = 0;
    while ((ent = readdir(d)) != NULL) {
        // skip .
        if (strcmp(ent->d_name, ".") == 0) continue;
        // create entry string; append '/' for directories
        char full[1024]; snprintf(full, sizeof(full), "%s%s", path, ent->d_name);
        struct stat st;
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
            // OPTIMIZED: Skip nested opendir() checks - these cause UI freezes on large directories
            // Icon type will be determined lazily by the UI renderer using the icon cache
            size_t len = strlen(ent->d_name) + 2;
            char *s = malloc(len);
            snprintf(s, len, "%s/", ent->d_name);
            lines = realloc(lines, sizeof(char*) * (count + 1)); lines[count++] = s;
        } else {
            char *s = strdup(ent->d_name);
            lines = realloc(lines, sizeof(char*) * (count + 1)); lines[count++] = s;
        }
    }
    closedir(d);
    // add parent entry if not root
    if (strcmp(canon, "sdmc:/") != 0) {
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
    // OPTIMIZED: Avoid blocking list_directory() - signal refresh flag instead
    printf("\x1b[%d;1H", view_rows + 2);
    printf("Actions for %s: A=Install, B=Delete, X=Cancel           \n", fullpath);
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
            // delete file
            int del_result = remove(fullpath);
            if (del_result == 0) {
                // Success: signal refresh needed via negative total_lines flag
                // File_explorer will see this and trigger incremental refresh
                printf("\x1b[%d;1H", view_rows + 2);
                printf("File deleted. Refreshing directory...           \n");
                fflush(stdout);
                usleep(500000); // brief feedback (500ms)
                *total_lines = -1;  // negative flag = "refresh needed"
            } else {
                printf("\x1b[%d;1H", view_rows + 2);
                printf("Failed to delete file (errno %d)               \n", errno);
                fflush(stdout);
                usleep(1000000);
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

char* fs_select_directory(const char* prompt) {
    (void)prompt;
    // Minimal fallback: return sdmc root. Caller must free.
    const char* fallback = "sdmc:/";
    char* out = malloc(strlen(fallback) + 1);
    if (!out) return NULL;
    strcpy(out, fallback);
    return out;
}
