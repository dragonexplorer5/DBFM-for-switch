/* Minimal, self-contained logger implementation.
 * Provides a tiny, robust logger used during build-fix iterations.
 */

#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#ifndef LOG_DIR
#define LOG_DIR "sdmc:/switch/DBFM/logs"
#endif
#ifndef CURRENT_LOG_PATH
#define CURRENT_LOG_PATH LOG_DIR "/current.log"
#endif
#define MAX_LOG_SIZE (512 * 1024)
/* Minimal, self-contained logger implementation.
 * Provides a tiny, robust logger used during build-fix iterations.
 */

#include "logger.h"
#include "ui.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#ifndef LOG_DIR
#define LOG_DIR "sdmc:/switch/DBFM/logs"
#endif
#ifndef CURRENT_LOG_PATH
#define CURRENT_LOG_PATH LOG_DIR "/current.log"
#endif
#define MAX_LOG_SIZE (512 * 1024)

static FILE* s_log_file = NULL;
static unsigned long long s_log_size = 0;

static void format_time(time_t ts, char* out, size_t outsz) {
    struct tm tm_info;
    localtime_r(&ts, &tm_info);
    strftime(out, outsz, "%Y-%m-%d %H:%M:%S", &tm_info);
}

Result logger_init(void) {
    // best-effort create directory
    mkdir(LOG_DIR, 0777);
    // Try opening the preferred path; if that fails, try a couple of
    // well-known fallbacks, then fall back to stdout so the app can
    // continue even without file logging.
    const char* try_paths[] = { CURRENT_LOG_PATH, "sdmc:/dbfm/logs/current.log", "sdmc:/dbfm/current.log", NULL };
    const char** p = try_paths;
    s_log_file = NULL;
    while (*p) {
        // best-effort mkdir for parent directory (ignore errors)
        mkdir(LOG_DIR, 0777);
        s_log_file = fopen(*p, "a+");
        if (s_log_file) break;
        p++;
    }
    if (!s_log_file) {
        // Last-resort: use stdout so logging calls still succeed
        s_log_file = stdout;
        s_log_size = 0;
        return 0;
    }
    fseek(s_log_file, 0, SEEK_END);
    s_log_size = (unsigned long long)ftell(s_log_file);
    return 0;
}

void logger_exit(void) {
    if (s_log_file) {
        fflush(s_log_file);
        fclose(s_log_file);
        s_log_file = NULL;
        s_log_size = 0;
    }
}

static Result rotate_logs_if_needed(size_t incoming) {
    if (!s_log_file) return (Result)-1;
    // If logging to stdout, skip rotation
    if (s_log_file == stdout) return 0;
    if (s_log_size + incoming <= MAX_LOG_SIZE) return 0;
    // rotate
    char backup[512];
    time_t now = time(NULL);
    char ts[64];
    format_time(now, ts, sizeof(ts));
    // sanitize ':' in timestamp for filenames
    for (char* p = ts; *p; ++p) if (*p == ':') *p = '-';
    snprintf(backup, sizeof(backup), "%s/log_%s.log", LOG_DIR, ts);
    fclose(s_log_file);
    rename(CURRENT_LOG_PATH, backup);
    s_log_file = fopen(CURRENT_LOG_PATH, "w");
    if (!s_log_file) return (Result)-1;
    s_log_size = 0;
    return 0;
}

Result log_event(LogLevel level, const char* fmt, ...) {
    if (!s_log_file) return (Result)-1;
    char timebuf[64];
    time_t now = time(NULL);
    format_time(now, timebuf, sizeof(timebuf));

    const char* level_str = "INFO";
    switch (level) {
        case LOG_DEBUG: level_str = "DEBUG"; break;
        case LOG_INFO: level_str = "INFO"; break;
        case LOG_WARN: level_str = "WARN"; break;
        case LOG_ERROR: level_str = "ERROR"; break;
        default: level_str = "INFO"; break;
    }

    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    char out[2048];
    int n = snprintf(out, sizeof(out), "[%s] [%s] %s\n", timebuf, level_str, msg);
    if (n <= 0) return (Result)-1;
    if (rotate_logs_if_needed((size_t)n) != 0) return (Result)-1;
    if (fputs(out, s_log_file) < 0) return (Result)-1;
    if (s_log_file != stdout) fflush(s_log_file);
    s_log_size += (unsigned long long)n;
    return 0;
}

Result logger_export_to_file(const char* path) {
    if (!path) return (Result)-1;
    FILE* src = fopen(CURRENT_LOG_PATH, "r");
    if (!src) return (Result)-1;
    FILE* dst = fopen(path, "w");
    if (!dst) { fclose(src); return (Result)-1; }
    char buf[4096];
    size_t r;
    while ((r = fread(buf, 1, sizeof(buf), src)) > 0) fwrite(buf, 1, r, dst);
    fclose(src);
    fclose(dst);
    return 0;
}

// Simple UI entry point for viewing logs in-app. Small stub used while
// developing/build-fix iterations; a full viewer can be implemented later.
void logger_show_viewer(void) {
    ui_show_message("System Log", "Log viewer not implemented");
}
