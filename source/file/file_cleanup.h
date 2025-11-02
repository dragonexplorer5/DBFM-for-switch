#ifndef FILE_CLEANUP_H
#define FILE_CLEANUP_H

#include <switch.h>
#include <time.h>
#include "crypto.h"
#include "secure_validation.h"

// File types to clean
typedef enum {
    CLEANUP_TEMP_FILES      = 1 << 0,
    CLEANUP_PARTIAL_DUMPS   = 1 << 1,
    CLEANUP_OLD_BACKUPS     = 1 << 2,
    CLEANUP_INSTALLED_NSP   = 1 << 3,
    CLEANUP_EMPTY_DIRS      = 1 << 4,
    CLEANUP_CORRUPT_FILES   = 1 << 5,
    CLEANUP_CACHE_FILES     = 1 << 6,
    CLEANUP_LOG_FILES       = 1 << 7,
    CLEANUP_ALL            = 0xFFFFFFFF
} CleanupFlags;

// File patterns to match
typedef struct {
    char name[64];
    char pattern[256];
    bool use_regex;
    CleanupFlags type;
} CleanupPattern;

// Cleanup statistics
typedef struct {
    size_t files_checked;
    size_t files_cleaned;
    size_t dirs_cleaned;
    size_t bytes_freed;
    size_t errors_encountered;
    time_t start_time;
    time_t end_time;
} CleanupStats;

// Cleanup configuration
typedef struct {
    CleanupFlags flags;
    bool secure_delete;
    bool verify_before_delete;
    bool auto_cleanup;
    
    // File age thresholds (in seconds)
    time_t temp_age_threshold;
    time_t backup_age_threshold;
    time_t log_age_threshold;
    time_t cache_age_threshold;
    
    // Count limits
    int keep_backup_count;
    int keep_log_count;
    size_t min_free_space;
    
    // Patterns
    CleanupPattern* patterns;
    size_t pattern_count;
    
    // Paths
    char backup_dir[PATH_MAX];
    char temp_dir[PATH_MAX];
    char log_dir[PATH_MAX];
    char cache_dir[PATH_MAX];
    
    // Validation
    ValidationFlags validation_flags;
} CleanupConfig;

// Initialize/cleanup
Result cleanup_init(void);
void cleanup_exit(void);

// Configuration management
void cleanup_config_init(CleanupConfig* config);
Result cleanup_config_load(CleanupConfig* config);
Result cleanup_config_save(const CleanupConfig* config);
Result cleanup_add_pattern(CleanupConfig* config, const CleanupPattern* pattern);

// Main cleanup operations
Result cleanup_run(const CleanupConfig* config, CleanupStats* stats,
                  void (*progress_cb)(const char* status, size_t current, size_t total));
Result cleanup_scan_directory(const char* path, const CleanupConfig* config,
                            CleanupStats* stats);
Result cleanup_cancel(void);

// Specific cleanup tasks
Result cleanup_temp_files(const char* path, time_t age_threshold,
                         void (*progress_cb)(const char* status, size_t current, size_t total));
Result cleanup_partial_dumps(const char* path,
                           void (*progress_cb)(const char* status, size_t current, size_t total));
Result cleanup_old_backups(const char* path, int keep_count, time_t threshold,
                          void (*progress_cb)(const char* status, size_t current, size_t total));
Result cleanup_installed_packages(const char* path,
                                void (*progress_cb)(const char* status, size_t current, size_t total));
Result cleanup_empty_directories(const char* path);
Result cleanup_corrupt_files(const char* path, ValidationFlags flags);
Result cleanup_old_logs(const char* path, int keep_count, time_t threshold);
Result cleanup_old_cache(const char* path, time_t threshold);

// Space management
Result cleanup_ensure_free_space(const char* path, size_t required_bytes);
Result cleanup_get_space_info(const char* path, u64* free_space, u64* total_space);

// File validation
bool cleanup_is_temp_file(const char* path);
bool cleanup_is_partial_dump(const char* path);
bool cleanup_is_installed_title(const char* nsp_path);
bool cleanup_is_old_backup(const char* backup_path, time_t threshold);
bool cleanup_is_corrupt_file(const char* path, ValidationFlags flags);
bool cleanup_matches_pattern(const char* path, const CleanupPattern* pattern);

// Statistics and reporting
void cleanup_stats_init(CleanupStats* stats);
Result cleanup_save_report(const CleanupStats* stats, const char* report_path);
const char* cleanup_get_status_string(Result rc);

// UI helpers
void cleanup_render_progress(const CleanupStats* stats);
void cleanup_render_summary(const CleanupStats* stats);
void cleanup_show_confirmation(const char* path, bool* confirmed);

#endif // FILE_CLEANUP_H