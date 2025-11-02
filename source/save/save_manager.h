#ifndef SAVE_MANAGER_H
#define SAVE_MANAGER_H

#include <switch.h>
#include "crypto.h"
#include "secure_validation.h"

// Save metadata
typedef struct {
    u64 title_id;
    u128 user_id;
    char title_name[256];
    char user_name[32];
    char title_author[128];
    char title_version[32];
    u64 save_id;
    size_t save_size;
    time_t modification_time;
    bool has_backup;
    char latest_backup[PATH_MAX];
    u32 backup_count;
} SaveMetadata;

// Save backup entry
typedef struct {
    char path[PATH_MAX];
    time_t backup_time;
    size_t size;
    char version[32];
    bool compressed;
    bool encrypted;
    bool verified;
} SaveBackup;

// Save configuration
typedef struct {
    char backup_path[PATH_MAX];
    bool compress_saves;
    bool encrypt_saves;
    bool verify_backup;
    bool backup_screenshots;
    bool backup_user_data;
    size_t max_backups_per_save;
    ValidationFlags validation_flags;
    char encryption_key[65];
} SaveConfig;

// Initialize/cleanup
Result save_manager_init(void);
void save_manager_exit(void);

// Basic save operations
Result save_backup_all(const char* backup_path,
                      void (*progress_cb)(const char* status, size_t current, size_t total));
Result save_backup_title(u64 title_id, const char* backup_path,
                        void (*progress_cb)(const char* status, size_t current, size_t total));
Result save_restore_all(const char* backup_path,
                       void (*progress_cb)(const char* status, size_t current, size_t total));
Result save_restore_title(u64 title_id, const char* backup_path,
                         void (*progress_cb)(const char* status, size_t current, size_t total));

// Enhanced save operations
Result save_backup_user_saves(u128 user_id, const char* backup_path,
                            void (*progress_cb)(const char* status, size_t current, size_t total));
Result save_export_decrypted(const SaveMetadata* save, const char* export_path,
                            void (*progress_cb)(const char* status, size_t current, size_t total));
Result save_import_save(const char* save_path, u64 title_id, u128 user_id,
                       void (*progress_cb)(const char* status, size_t current, size_t total));
Result save_verify_save(const SaveMetadata* save);
Result save_delete_save(const SaveMetadata* save);

// Save enumeration
Result save_list_titles(char*** out_titles, int* out_count);
Result save_list_saves(SaveMetadata** saves, size_t* count);
Result save_list_saves_for_title(u64 title_id, SaveMetadata** saves, size_t* count);
Result save_list_saves_for_user(u128 user_id, SaveMetadata** saves, size_t* count);
Result save_get_title_name(u64 title_id, char* out_name, size_t name_size);
Result save_get_metadata(u64 title_id, u128 user_id, SaveMetadata* metadata);

// Backup management
Result save_list_backups(const SaveMetadata* save, SaveBackup** backups, size_t* count);
Result save_get_backup_info(const char* backup_path, SaveBackup* backup);
Result save_delete_backup(const char* backup_path);
Result save_verify_backup(const char* backup_path);
Result save_cleanup_old_backups(const SaveConfig* config);

// Configuration
Result save_load_config(SaveConfig* config);
Result save_save_config(const SaveConfig* config);
Result save_set_config(const SaveConfig* config);
Result save_get_config(SaveConfig* config);

// User management
Result save_list_users(u128** user_ids, size_t* count);
Result save_get_user_name(u128 user_id, char* name, size_t name_size);
Result save_get_user_saves_size(u128 user_id, size_t* total_size);

// Space management
Result save_get_free_space(u64* free_bytes);
Result save_get_total_space(u64* total_bytes);
Result save_estimate_backup_size(const SaveMetadata* save, size_t* size);

// UI helpers
void save_render_title_list(int start_row, int selected_row,
                           const SaveMetadata* saves, size_t count);
void save_render_backup_list(int start_row, int selected_row,
                           const SaveBackup* backups, size_t count);
void save_render_save_details(const SaveMetadata* save);
void save_render_user_list(int start_row, int selected_row,
                          const u128* user_ids, const char** user_names, size_t count);

// Error handling
const char* save_get_error(Result rc);

#endif // SAVE_MANAGER_H