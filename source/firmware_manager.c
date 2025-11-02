#include "firmware_manager.h"
#include "fs.h"
#include "task_queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIRMWARE_MOUNT_POINT "sdmc:/firmware"
#define FIRMWARE_BUFFER_SIZE 0x800000 // 8MB buffer for copying

// System partition paths
static const char* SYSTEM_PARTITIONS[] = {
    "BCPKG2-1-Normal-Main",
    "BCPKG2-2-Normal-Sub",
    "BCPKG2-3-SafeMode-Main",
    "BCPKG2-4-SafeMode-Sub",
    "BCPKG2-5-Repair-Main",
    "BCPKG2-6-Repair-Sub"
};

static const size_t NUM_SYSTEM_PARTITIONS = sizeof(SYSTEM_PARTITIONS) / sizeof(char*);

// Internal context
static FsFileSystem s_firmwareFs;
static bool s_initialized = false;

// Mount firmware partition
static Result _mount_firmware(void) {
    Result rc = fsOpenBisFileSystem(&s_firmwareFs, FsBisPartitionId_System, "");
    if (R_SUCCEEDED(rc)) {
        rc = fsMountSystemSaveData(&s_firmwareFs, SaveDataSpaceId_System, 0x8000000000000000);
    }
    return rc;
}

// Unmount firmware partition
static void _unmount_firmware(void) {
    fsFsUnmountDevice(&s_firmwareFs, FIRMWARE_MOUNT_POINT);
    fsFileSystemClose(&s_firmwareFs);
}

Result firmware_init(void) {
    if (s_initialized) return 0;
    
    Result rc = _mount_firmware();
    if (R_SUCCEEDED(rc)) {
        s_initialized = true;
    }
    return rc;
}

void firmware_exit(void) {
    if (s_initialized) {
        _unmount_firmware();
        s_initialized = false;
    }
}

Result firmware_get_version(FirmwareInfo* out_info) {
    if (!s_initialized || !out_info) {
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    }

    SetSysFirmwareVersion fw;
    Result rc = setsysGetFirmwareVersion(&fw);
    if (R_FAILED(rc)) return rc;

    memset(out_info, 0, sizeof(FirmwareInfo));
    out_info->version_major = fw.major;
    out_info->version_minor = fw.minor;
    out_info->version_micro = fw.micro;
    out_info->version_padded = fw.pad;
    snprintf(out_info->version_string, sizeof(out_info->version_string),
             "%u.%u.%u", fw.major, fw.minor, fw.micro);

    return 0;
}

Result firmware_export(const char* output_path, bool include_exfat,
                      void (*progress_callback)(size_t current, size_t total)) {
    if (!s_initialized || !output_path) {
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    }

    // Create output directory if it doesn't exist
    char dir_path[FS_MAX_PATH];
    snprintf(dir_path, sizeof(dir_path), "%s", output_path);
    fs_create_directories(dir_path);

    size_t total_size = 0;
    size_t current_size = 0;

    // First pass: calculate total size
    for (size_t i = 0; i < NUM_SYSTEM_PARTITIONS; i++) {
        char src_path[FS_MAX_PATH];
        snprintf(src_path, sizeof(src_path), "%s/%s", 
                FIRMWARE_MOUNT_POINT, SYSTEM_PARTITIONS[i]);

        struct stat st;
        if (stat(src_path, &st) == 0) {
            total_size += st.st_size;
        }
    }

    // Second pass: copy files
    for (size_t i = 0; i < NUM_SYSTEM_PARTITIONS; i++) {
        char src_path[FS_MAX_PATH];
        char dst_path[FS_MAX_PATH];
        
        snprintf(src_path, sizeof(src_path), "%s/%s", 
                FIRMWARE_MOUNT_POINT, SYSTEM_PARTITIONS[i]);
        snprintf(dst_path, sizeof(dst_path), "%s/%s.bin", 
                output_path, SYSTEM_PARTITIONS[i]);

        FILE* src = fopen(src_path, "rb");
        if (!src) continue;

        FILE* dst = fopen(dst_path, "wb");
        if (!dst) {
            fclose(src);
            continue;
        }

        u8* buffer = malloc(FIRMWARE_BUFFER_SIZE);
        if (!buffer) {
            fclose(src);
            fclose(dst);
            continue;
        }

        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, FIRMWARE_BUFFER_SIZE, src)) > 0) {
            fwrite(buffer, 1, bytes_read, dst);
            current_size += bytes_read;
            
            if (progress_callback) {
                progress_callback(current_size, total_size);
            }
        }

        free(buffer);
        fclose(src);
        fclose(dst);
    }

    // Export ExFAT driver if requested
    if (include_exfat) {
        char exfat_src[FS_MAX_PATH];
        char exfat_dst[FS_MAX_PATH];
        
        snprintf(exfat_src, sizeof(exfat_src), "%s/exfat_driver",
                FIRMWARE_MOUNT_POINT);
        snprintf(exfat_dst, sizeof(exfat_dst), "%s/exfat_driver.bin",
                output_path);

        FILE* src = fopen(exfat_src, "rb");
        if (src) {
            FILE* dst = fopen(exfat_dst, "wb");
            if (dst) {
                u8* buffer = malloc(FIRMWARE_BUFFER_SIZE);
                if (buffer) {
                    size_t bytes_read;
                    while ((bytes_read = fread(buffer, 1, FIRMWARE_BUFFER_SIZE, src)) > 0) {
                        fwrite(buffer, 1, bytes_read, dst);
                    }
                    free(buffer);
                }
                fclose(dst);
            }
            fclose(src);
        }
    }

    return 0;
}

Result firmware_verify_package(const char* package_path, FirmwareInfo* out_info) {
    if (!s_initialized || !package_path || !out_info) {
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    }

    memset(out_info, 0, sizeof(FirmwareInfo));
    
    // Check for required firmware files
    bool all_files_present = true;
    out_info->package_size = 0;

    for (size_t i = 0; i < NUM_SYSTEM_PARTITIONS; i++) {
        char path[FS_MAX_PATH];
        snprintf(path, sizeof(path), "%s/%s.bin", package_path, SYSTEM_PARTITIONS[i]);
        
        struct stat st;
        if (stat(path, &st) != 0) {
            all_files_present = false;
            break;
        }
        out_info->package_size += st.st_size;
    }

    if (!all_files_present) {
        return MAKERESULT(Module_Libnx, LibnxError_NotFound);
    }

    // Check for ExFAT driver
    char exfat_path[FS_MAX_PATH];
    snprintf(exfat_path, sizeof(exfat_path), "%s/exfat_driver.bin", package_path);
    
    struct stat st;
    out_info->is_exfat = (stat(exfat_path, &st) == 0);
    if (out_info->is_exfat) {
        out_info->package_size += st.st_size;
    }

    // Try to determine version from package contents
    // This is a simplified version - in practice, you'd need to parse the
    // firmware files to get the actual version
    char version_path[FS_MAX_PATH];
    snprintf(version_path, sizeof(version_path), "%s/version.txt", package_path);
    
    FILE* f = fopen(version_path, "r");
    if (f) {
        fscanf(f, "%u.%u.%u", &out_info->version_major,
               &out_info->version_minor, &out_info->version_micro);
        fclose(f);
    }

    return 0;
}

Result firmware_list_contents(char*** out_content_paths, size_t* out_count) {
    if (!s_initialized || !out_content_paths || !out_count) {
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    }

    FsDir dir;
    Result rc = fsFsOpenDirectory(&s_firmwareFs, "/", 
                                FsDirOpenMode_ReadFiles, &dir);
    if (R_FAILED(rc)) return rc;

    size_t capacity = 16;
    size_t count = 0;
    char** paths = malloc(capacity * sizeof(char*));
    
    if (!paths) {
        fsDirClose(&dir);
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }

    s64 total_entries = 0;
    rc = fsDirGetEntryCount(&dir, &total_entries);
    if (R_SUCCEEDED(rc)) {
        FsDirectoryEntry entry;
        while (R_SUCCEEDED(fsDirRead(&dir, &entry)) && entry.name[0] != '\0') {
            if (count >= capacity) {
                capacity *= 2;
                char** new_paths = realloc(paths, capacity * sizeof(char*));
                if (!new_paths) {
                    for (size_t i = 0; i < count; i++) {
                        free(paths[i]);
                    }
                    free(paths);
                    fsDirClose(&dir);
                    return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
                }
                paths = new_paths;
            }

            paths[count] = strdup(entry.name);
            if (!paths[count]) {
                for (size_t i = 0; i < count; i++) {
                    free(paths[i]);
                }
                free(paths);
                fsDirClose(&dir);
                return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
            }
            count++;
        }
    }

    fsDirClose(&dir);
    *out_content_paths = paths;
    *out_count = count;
    return 0;
}

void firmware_free_content_list(char** content_paths, size_t count) {
    if (content_paths) {
        for (size_t i = 0; i < count; i++) {
            free(content_paths[i]);
        }
        free(content_paths);
    }
}

Result firmware_extract_file(const char* content_path, const char* output_path) {
    if (!s_initialized || !content_path || !output_path) {
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    }

    char src_path[FS_MAX_PATH];
    snprintf(src_path, sizeof(src_path), "%s/%s", 
             FIRMWARE_MOUNT_POINT, content_path);

    FILE* src = fopen(src_path, "rb");
    if (!src) {
        return MAKERESULT(Module_Libnx, LibnxError_NotFound);
    }

    FILE* dst = fopen(output_path, "wb");
    if (!dst) {
        fclose(src);
        return MAKERESULT(Module_Libnx, LibnxError_IoError);
    }

    u8* buffer = malloc(FIRMWARE_BUFFER_SIZE);
    if (!buffer) {
        fclose(src);
        fclose(dst);
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }

    size_t bytes_read;
    Result rc = 0;

    while ((bytes_read = fread(buffer, 1, FIRMWARE_BUFFER_SIZE, src)) > 0) {
        if (fwrite(buffer, 1, bytes_read, dst) != bytes_read) {
            rc = MAKERESULT(Module_Libnx, LibnxError_IoError);
            break;
        }
    }

    free(buffer);
    fclose(src);
    fclose(dst);
    return rc;
}

const char* firmware_get_error_msg(Result rc) {
    if (R_SUCCEEDED(rc)) return "Success";

    switch (rc) {
        case MAKERESULT(Module_Libnx, LibnxError_NotInitialized):
            return "Firmware manager not initialized";
        case MAKERESULT(Module_Libnx, LibnxError_NotFound):
            return "Firmware file not found";
        case MAKERESULT(Module_Libnx, LibnxError_IoError):
            return "I/O error during firmware operation";
        case MAKERESULT(Module_Libnx, LibnxError_OutOfMemory):
            return "Out of memory";
        default:
            return "Unknown error";
    }
}