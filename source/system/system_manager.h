#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include <switch.h>
#include "crypto.h"
#include "secure_validation.h"

// NAND partition types
typedef enum {
    NAND_PARTITION_BOOT0,
    NAND_PARTITION_BOOT1,
    NAND_PARTITION_RAWNAND,
    NAND_PARTITION_USER,
    NAND_PARTITION_SYSTEM,
    NAND_PARTITION_SAFE
} NandPartition;

// emuMMC configuration
typedef struct {
    char name[64];
    char path[PATH_MAX];
    u64 sector_offset;
    size_t size;
    bool enabled;
    bool is_file_based;
    char nintendo_path[PATH_MAX];
    time_t created_time;
    char firmware_version[32];
} EmuMMCConfig;

// Backup configuration
typedef struct {
    bool verify_dump;
    bool split_files;
    size_t split_size;
    bool compress;
    ValidationFlags validation_flags;
    char backup_path[PATH_MAX];
    bool backup_saves;
    bool backup_user;
    bool encrypt_backup;
} BackupConfig;

// System information
typedef struct {
    char serial[32];
    u64 device_id;
    char firmware_version[32];
    u32 firmware_target;
    bool auto_rcm_enabled;
    bool emummc_enabled;
    u64 free_space[6];  // One for each partition
    u64 total_space[6]; // One for each partition
    u32 key_generation;
    bool keys_valid;
    bool fuses_valid;
} SystemInfo;

// Initialize/cleanup
Result system_manager_init(void);
void system_manager_exit(void);

// NAND operations
Result system_dump_partition(NandPartition partition, const char* out_path, 
                           const BackupConfig* config,
                           void (*progress_cb)(const char* status, size_t current, size_t total));
Result system_restore_partition(NandPartition partition, const char* backup_path,
                              const BackupConfig* config,
                              void (*progress_cb)(const char* status, size_t current, size_t total));
Result system_verify_partition(NandPartition partition);
Result system_dump_all(const char* base_path, const BackupConfig* config,
                      void (*progress_cb)(const char* status, size_t current, size_t total));
Result system_restore_all(const char* backup_path, const BackupConfig* config,
                         void (*progress_cb)(const char* status, size_t current, size_t total));

// emuMMC management
Result emummc_create(const char* path, const EmuMMCConfig* config,
                    void (*progress_cb)(const char* status, size_t current, size_t total));
Result emummc_delete(const char* name);
Result emummc_enable(const char* name, bool enable);
Result emummc_dump(const char* name, const char* dump_path,
                  void (*progress_cb)(const char* status, size_t current, size_t total));
Result emummc_restore(const char* name, const char* dump_path,
                     void (*progress_cb)(const char* status, size_t current, size_t total));
Result emummc_verify(const char* name);
Result emummc_list(EmuMMCConfig** configs, size_t* count);
Result emummc_get_info(const char* name, EmuMMCConfig* config);

// System configuration
Result system_toggle_auto_rcm(bool enable);
Result system_get_info(SystemInfo* info);
Result system_verify_firmware(void);
Result system_verify_keys(void);
Result system_dump_keys(const char* out_path);
Result system_import_keys(const char* key_path);

// Space management
Result system_get_free_space(NandPartition partition, u64* free_bytes);
Result system_get_total_space(NandPartition partition, u64* total_bytes);
Result system_cleanup_temp(void);
Result system_optimize_space(NandPartition partition);

// Backup management
Result system_create_backup(const BackupConfig* config,
                          void (*progress_cb)(const char* status, size_t current, size_t total));
Result system_verify_backup(const char* backup_path);
Result system_list_backups(char*** backups, size_t* count);
Result system_delete_backup(const char* backup_path);

// UI integration
void system_manager_show_menu(void);
void system_render_info(const SystemInfo* info);
void system_render_emummc_list(int start_row, int selected_row,
                              const EmuMMCConfig* configs, size_t count);
void system_render_backup_list(int start_row, int selected_row,
                             const char** backups, size_t count);
void system_render_partition_info(NandPartition partition);

// Error handling
const char* system_get_error(Result rc);

#endif // SYSTEM_MANAGER_H

#endif // SYSTEM_MANAGER_H