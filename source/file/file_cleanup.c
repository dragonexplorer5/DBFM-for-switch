#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "file_cleanup.h"
#include "task_queue.h"
#include "nsp_manager.h"

static size_t total_freed = 0;

void cleanup_config_init(CleanupConfig* config) {
    if (!config) return;
    config->flags = CLEANUP_TEMP_FILES | CLEANUP_PARTIAL_DUMPS | CLEANUP_OLD_BACKUPS | CLEANUP_INSTALLED_NSP;
    config->secure_delete = false;
    config->verify_before_delete = false;
    config->auto_cleanup = false;
    config->temp_age_threshold = time(NULL) - (7 * 24 * 60 * 60); // 7 days default for temp files
    config->backup_age_threshold = time(NULL) - (30 * 24 * 60 * 60); // 30 days
    config->log_age_threshold = time(NULL) - (30 * 24 * 60 * 60);
    config->cache_age_threshold = time(NULL) - (7 * 24 * 60 * 60);
    config->keep_backup_count = 3;
    config->keep_log_count = 10;
    config->min_free_space = 0;
    config->patterns = NULL; config->pattern_count = 0;
    config->backup_dir[0] = '\0'; config->temp_dir[0] = '\0'; config->log_dir[0] = '\0'; config->cache_dir[0] = '\0';
    config->validation_flags = 0;
}

static Result process_directory(const char* path, const CleanupConfig* config,
                             void (*callback)(const char*, const struct stat*, void*),
                             void* user_data) {
    DIR* dir = opendir(path);
    if (!dir) return -1;
    
    struct dirent* entry;
    char full_path[PATH_MAX];
    struct stat st;
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                process_directory(full_path, config, callback, user_data);
            } else {
                callback(full_path, &st, user_data);
            }
        }
    }
    
    closedir(dir);
    return 0;
}

static void temp_file_callback(const char* path, const struct stat* st, void* user_data) {
    const CleanupConfig* config = (const CleanupConfig*)user_data;
    (void)config;
    if (cleanup_is_temp_file(path)) {
        task_queue_add(TASK_DELETE, path, NULL);
        total_freed += (size_t)st->st_size;
    }
}

static void partial_dump_callback(const char* path, const struct stat* st, void* user_data) {
    const CleanupConfig* config = (const CleanupConfig*)user_data;
    (void)config;
    if (cleanup_is_partial_dump(path)) {
        task_queue_add(TASK_DELETE, path, NULL);
        total_freed += (size_t)st->st_size;
    }
}

static void old_backup_callback(const char* path, const struct stat* st, void* user_data) {
    const CleanupConfig* config = (const CleanupConfig*)user_data;
    if (cleanup_is_old_backup(path, config->backup_age_threshold)) {
        task_queue_add(TASK_DELETE, path, NULL);
        total_freed += (size_t)st->st_size;
    }
}

static void installed_nsp_callback(const char* path, const struct stat* st, void* user_data) {
    const CleanupConfig* config = (const CleanupConfig*)user_data;
    (void)config;
    if (strstr(path, ".nsp") && cleanup_is_installed_title(path)) {
        task_queue_add(TASK_DELETE, path, NULL);
        total_freed += (size_t)st->st_size;
    }
}

Result cleanup_scan_directory(const char* path, const CleanupConfig* config, CleanupStats* stats) {
    Result rc = 0;
    total_freed = 0;
    (void)stats;

    if (!config) return MAKERESULT(Module_Libnx, LibnxError_BadInput);

    // Process each cleanup task based on config flags
    if (config->flags & CLEANUP_TEMP_FILES) {
        rc = process_directory(path, config, temp_file_callback, (void*)config);
        if (R_FAILED(rc)) return rc;
    }

    if (config->flags & CLEANUP_PARTIAL_DUMPS) {
        rc = process_directory(path, config, partial_dump_callback, (void*)config);
        if (R_FAILED(rc)) return rc;
    }

    if (config->flags & CLEANUP_OLD_BACKUPS) {
        rc = process_directory(path, config, old_backup_callback, (void*)config);
        if (R_FAILED(rc)) return rc;
    }

    if (config->flags & CLEANUP_INSTALLED_NSP) {
        rc = process_directory(path, config, installed_nsp_callback, (void*)config);
        if (R_FAILED(rc)) return rc;
    }

    return rc;
}

Result cleanup_temp_files(const char* path, time_t age_threshold, void (*progress_cb)(const char*, size_t, size_t)) {
    CleanupConfig config;
    cleanup_config_init(&config);
    config.flags = CLEANUP_TEMP_FILES;
    if (age_threshold > 0) config.temp_age_threshold = age_threshold;
    (void)progress_cb;
    return cleanup_scan_directory(path, &config, NULL);
}

Result cleanup_partial_dumps(const char* path, void (*progress_cb)(const char*, size_t, size_t)) {
    CleanupConfig config;
    cleanup_config_init(&config);
    config.flags = CLEANUP_PARTIAL_DUMPS;
    (void)progress_cb;
    return cleanup_scan_directory(path, &config, NULL);
}

Result cleanup_old_backups(const char* path, int keep_count, time_t threshold, void (*progress_cb)(const char*, size_t, size_t)) {
    CleanupConfig config;
    cleanup_config_init(&config);
    config.flags = CLEANUP_OLD_BACKUPS;
    config.keep_backup_count = keep_count;
    if (threshold > 0) config.backup_age_threshold = threshold;
    (void)progress_cb;
    return cleanup_scan_directory(path, &config, NULL);
}

Result cleanup_installed_packages(const char* path, void (*progress_cb)(const char*, size_t, size_t)) {
    CleanupConfig config;
    cleanup_config_init(&config);
    config.flags = CLEANUP_INSTALLED_NSP;
    (void)progress_cb;
    return cleanup_scan_directory(path, &config, NULL);
}

bool cleanup_is_installed_title(const char* nsp_path) {
    // Extract title ID from NSP filename (assumes format: titleid.nsp)
    const char* title_start = strrchr(nsp_path, '/');
    if (!title_start) return false;
    title_start++;
    
    char title_id_str[17] = {0};
    strncpy(title_id_str, title_start, 16);
    
    (void)title_id_str;
    (void)nsp_path;
    // For safety in this compatibility pass: avoid querying the system database.
    // Return false to avoid deleting NSPs that might be installed.
    return false;
}

bool cleanup_is_old_backup(const char* backup_path, time_t threshold) {
    struct stat st;
    if (stat(backup_path, &st) != 0) return false;
    return S_ISREG(st.st_mode) && st.st_mtime < threshold;
}

size_t get_total_freed_space(void) {
    return total_freed;
}

// Simple helpers matching header
bool cleanup_is_temp_file(const char* path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return false;
    return (strcasecmp(ext, ".tmp") == 0) || (strcasecmp(ext, ".temp") == 0);
}

bool cleanup_is_partial_dump(const char* path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return false;
    return (strcasecmp(ext, ".part") == 0) || (strcasecmp(ext, ".partial") == 0);
}