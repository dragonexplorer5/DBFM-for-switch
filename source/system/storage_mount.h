#ifndef STORAGE_MOUNT_H
#define STORAGE_MOUNT_H

#include <switch.h>

// Storage types
typedef enum {
    STORAGE_SD,
    STORAGE_NAND_USER,
    STORAGE_NAND_SYSTEM,
    STORAGE_NAND_SAFE,
    STORAGE_USB,
    STORAGE_REMOTE
} StorageType;

// Mount options
typedef struct {
    bool read_only;
    bool show_hidden;
    bool allow_system;
} MountOptions;

// Storage information
typedef struct {
    u64 total_space;
    u64 free_space;
    u64 used_space;
    char label[64];
    char fs_type[32];
} StorageInfo;

// Initialize storage subsystem
Result storage_init(void);
void storage_exit(void);

// Mount/unmount operations
Result storage_mount(StorageType type, const char* mount_point, MountOptions* options);
Result storage_unmount(StorageType type);
bool storage_is_mounted(StorageType type);

// Storage information
Result storage_get_info(StorageType type, StorageInfo* out_info);
const char* storage_get_mount_point(StorageType type);

// USB and remote connection
Result storage_init_usb(void);
Result storage_init_remote(const char* host, u16 port);
void storage_disconnect_usb(void);
void storage_disconnect_remote(void);

#endif // STORAGE_MOUNT_H