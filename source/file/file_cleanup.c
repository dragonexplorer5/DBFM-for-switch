#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "file_cleanup.h"
#include "task_queue.h"
#include "nsp_manager.h"

static size_t total_freed = 0;

void cleanup_config_init(CleanupConfig* config) {
    config->remove_temp_files = true;
    config->remove_partial_dumps = true;
    config->remove_old_backups = true;
    config->remove_installed_nsp = true;
    config->keep_backup_count = 3;
    config->backup_age_threshold = time(NULL) - (30 * 24 * 60 * 60); // 30 days
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
    
    if (is_temp_file(path)) {
        task_queue_add(TASK_DELETE, path, NULL);
        total_freed += st->st_size;
    }
}

static void partial_dump_callback(const char* path, const struct stat* st, void* user_data) {
    const CleanupConfig* config = (const CleanupConfig*)user_data;
    
    if (is_partial_dump(path, st->st_size)) {
        task_queue_add(TASK_DELETE, path, NULL);
        total_freed += st->st_size;
    }
}

static void old_backup_callback(const char* path, const struct stat* st, void* user_data) {
    const CleanupConfig* config = (const CleanupConfig*)user_data;
    
    if (is_old_backup(path, config->backup_age_threshold)) {
        task_queue_add(TASK_DELETE, path, NULL);
        total_freed += st->st_size;
    }
}

static void installed_nsp_callback(const char* path, const struct stat* st, void* user_data) {
    const CleanupConfig* config = (const CleanupConfig*)user_data;
    
    if (strstr(path, ".nsp") && is_installed_title(path)) {
        task_queue_add(TASK_DELETE, path, NULL);
        total_freed += st->st_size;
    }
}

Result cleanup_scan_directory(const char* path, const CleanupConfig* config) {
    Result rc = 0;
    total_freed = 0;
    
    // Process each cleanup task based on config
    if (config->remove_temp_files) {
        rc = process_directory(path, config, temp_file_callback, (void*)config);
        if (R_FAILED(rc)) return rc;
    }
    
    if (config->remove_partial_dumps) {
        rc = process_directory(path, config, partial_dump_callback, (void*)config);
        if (R_FAILED(rc)) return rc;
    }
    
    if (config->remove_old_backups) {
        rc = process_directory(path, config, old_backup_callback, (void*)config);
        if (R_FAILED(rc)) return rc;
    }
    
    if (config->remove_installed_nsp) {
        rc = process_directory(path, config, installed_nsp_callback, (void*)config);
        if (R_FAILED(rc)) return rc;
    }
    
    return rc;
}

Result cleanup_temp_files(const char* path) {
    CleanupConfig config;
    cleanup_config_init(&config);
    config.remove_temp_files = true;
    config.remove_partial_dumps = false;
    config.remove_old_backups = false;
    config.remove_installed_nsp = false;
    
    return cleanup_scan_directory(path, &config);
}

Result cleanup_partial_dumps(const char* path) {
    CleanupConfig config;
    cleanup_config_init(&config);
    config.remove_temp_files = false;
    config.remove_partial_dumps = true;
    config.remove_old_backups = false;
    config.remove_installed_nsp = false;
    
    return cleanup_scan_directory(path, &config);
}

Result cleanup_old_backups(const char* path, int keep_count, time_t threshold) {
    CleanupConfig config;
    cleanup_config_init(&config);
    config.remove_temp_files = false;
    config.remove_partial_dumps = false;
    config.remove_old_backups = true;
    config.remove_installed_nsp = false;
    config.keep_backup_count = keep_count;
    config.backup_age_threshold = threshold;
    
    return cleanup_scan_directory(path, &config);
}

Result cleanup_installed_packages(const char* path) {
    CleanupConfig config;
    cleanup_config_init(&config);
    config.remove_temp_files = false;
    config.remove_partial_dumps = false;
    config.remove_old_backups = false;
    config.remove_installed_nsp = true;
    
    return cleanup_scan_directory(path, &config);
}

bool is_installed_title(const char* nsp_path) {
    // Extract title ID from NSP filename (assumes format: titleid.nsp)
    const char* title_start = strrchr(nsp_path, '/');
    if (!title_start) return false;
    title_start++;
    
    char title_id_str[17] = {0};
    strncpy(title_id_str, title_start, 16);
    
    u64 title_id = strtoull(title_id_str, NULL, 16);
    if (title_id == 0) return false;
    
    // Check if title is installed
    NcmContentMetaDatabase meta_db;
    Result rc = ncmOpenContentMetaDatabase(&meta_db, NcmStorageId_SdCard);
    if (R_FAILED(rc)) return false;
    
    bool installed = false;
    NcmContentMetaKey meta_key = {
        .id = title_id,
        .type = NcmContentMetaType_Application,
        .version = 0
    };
    
    s32 total = 0;
    rc = ncmContentMetaDatabaseList(&meta_db, &meta_key, 1, &total);
    installed = R_SUCCEEDED(rc) && total > 0;
    
    ncmContentMetaDatabaseClose(&meta_db);
    return installed;
}

bool is_old_backup(const char* backup_path, time_t threshold) {
    struct stat st;
    if (stat(backup_path, &st) != 0) return false;
    
    return S_ISREG(st.st_mode) && st.st_mtime < threshold;
}

size_t get_total_freed_space(void) {
    return total_freed;
}