#include <string.h>
#include <malloc.h>
#include "system_manager.h"
#include "ui.h"
#include "features/firmware_ui.h"
#include "firmware_manager.h"
#include "../file/fs.h"
#include <stdio.h>
#include <stdarg.h>

// Debug logging implementation
void system_log(int level, const char* fmt, ...) {
#ifdef DEBUG
    static const char* level_str[] = {
        "NONE", "ERROR", "INFO", "DEBUG"
    };
    
    if (level > 0 && level <= SYSTEM_LOG_DEBUG) {
        va_list args;
        printf("[SYSTEM-%s] ", level_str[level]);
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        printf("\n");
    }
#endif
}

// Get SOC temperature in millicelsius
int system_get_temperature(void) {
#ifdef __SWITCH__
    Result rc;
    s32 temperature = 0;
    
    rc = tsInitialize();
    if (R_SUCCEEDED(rc)) {
        rc = tsGetTemperatureMilliC(TsLocation_External, &temperature);
        tsExit();
        
        if (R_SUCCEEDED(rc)) {
            return (int)temperature;
        }
    }
#endif
    return -1;
}

int system_get_battery_percent(void) {
#ifdef __SWITCH__
    static Service psm_service;
    static bool psm_initialized = false;
    Result rc;
    u32 percent = 0;
    PsmChargerType charger_type;
    bool charger_connected = false;
    int retry_count = 0;
    
    // Check temperature first
    int temp = system_get_temperature();
    if (temp > SYSTEM_TEMP_CRITICAL) {
        system_log(SYSTEM_LOG_ERROR, "Temperature too high: %d°C", temp/1000);
        return -1;
    }

    // Initialize PSM service if needed
    if (!psm_initialized) {
        rc = psmInitialize();
        if (R_FAILED(rc)) {
            system_log(SYSTEM_LOG_ERROR, "Failed to initialize PSM: 0x%x", rc);
            return -1;
        }
    psm_initialized = true;
        system_log(SYSTEM_LOG_INFO, "PSM service initialized");
    }

    // Get battery percentage with retries
    while (retry_count < BATTERY_READ_RETRY_MAX) {
        rc = psmGetBatteryChargePercentage(&percent);
        if (R_SUCCEEDED(rc)) {
            break;
        }
        retry_count++;
        system_log(SYSTEM_LOG_DEBUG, "Battery read retry %d/3", retry_count);
        svcSleepThread(100000000ULL); // 100ms delay between retries
    }
    
    if (R_FAILED(rc)) {
        system_log(SYSTEM_LOG_ERROR, "Failed to get battery percentage after %d retries", retry_count);
        goto cleanup;
    }

    // Check charging state
    rc = psmGetChargerType(&charger_type);
    if (R_SUCCEEDED(rc)) {
        charger_connected = (charger_type != 0);
        system_log(SYSTEM_LOG_DEBUG, "Charger: %s, Type: %d", 
                  charger_connected ? "Connected" : "Disconnected",
                  charger_type);
    }

    // Validate and adjust percentage
    if (percent > 100) {
        system_log(SYSTEM_LOG_INFO, "Clamping battery percentage from %d to 100", percent);
        percent = 100;
    }

    // Report 100% when charging and nearly full
    if (charger_connected && percent >= BATTERY_FULLY_CHARGED) {
        system_log(SYSTEM_LOG_DEBUG, "Charging and >= %d%%, reporting 100%%", BATTERY_FULLY_CHARGED);
        percent = 100;
    }

    system_log(SYSTEM_LOG_INFO, "Battery: %d%% %s", 
              percent, charger_connected ? "(Charging)" : "");
    return (int)percent;

cleanup:
    if (psm_initialized) {
        psmExit();
        psm_initialized = false;
        system_log(SYSTEM_LOG_INFO, "PSM service cleaned up");
    }
    return -1;
#else
    system_log(SYSTEM_LOG_DEBUG, "Non-Switch platform, battery unavailable");
    return -1;
#endif
}

#define SECTOR_SIZE 0x200
#define BUFFER_SIZE (8 * 1024 * 1024)

static u8* transfer_buffer = NULL;

static Result initialize_transfer_buffer(void) {
    if (!transfer_buffer) {
        transfer_buffer = (u8*)memalign(0x1000, BUFFER_SIZE);
        if (!transfer_buffer) return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }
    return 0;
}

Result system_manager_init(void) {
    Result rc = firmware_ui_init();
    if (R_FAILED(rc)) return rc;
    return 0;
}

void system_manager_exit(void) {
    firmware_exit();
    if (transfer_buffer) {
        free(transfer_buffer);
        transfer_buffer = NULL;
    }
}

// Simplified space query helpers (compatibility shims)
Result system_get_free_space(NandPartition partition, u64* free_bytes) {
    if (free_bytes) *free_bytes = 0; // conservative default
    return 0;
}

Result system_get_total_space(NandPartition partition, u64* total_bytes) {
    if (total_bytes) *total_bytes = 0;
    return 0;
}

void system_manager_show_menu(void) {
    MenuItem items[] = {
        {"System Information", true},
        {"Firmware Management", true},
        {"NAND Operations", true},
        {"emuMMC Tools", true},
        {"Back", true}
    };

    while (1) {
        int selection = ui_show_menu("System Tools", items, 5);
        
        switch (selection) {
            case 0: {
                char info[1024];
                if (R_SUCCEEDED(system_get_info(info, sizeof(info)))) {
                    ui_show_message("System Information", info);
                }
                break;
            }
            case 1:
                firmware_ui_show_menu();
                break;
            case 2: {
                MenuItem nand_items[] = {
                    {"Dump NAND", true},
                    {"Restore NAND", true},
                    {"Dump BOOT0", true},
                    {"Dump BOOT1", true},
                    {"Back", true}
                };
                int nand_sel = ui_show_menu("NAND Operations", nand_items, 5);
                switch (nand_sel) {
                    case 0: {
                        char* path = fs_select_directory("Select NAND Dump Location");
                        if (path) {
                            Result rc = system_dump_nand(path);
                            if (R_SUCCEEDED(rc)) {
                                ui_show_message("Success", "NAND dumped successfully");
                            } else {
                                ui_show_error("Error", "Failed to dump NAND");
                            }
                            free(path);
                        }
                        break;
                    }
                    case 1: {
                        char* path = fs_select_directory("Select NAND Backup");
                        if (path) {
                            Result rc = system_restore_nand(path);
                            if (R_SUCCEEDED(rc)) {
                                ui_show_message("Success", "NAND restored successfully");
                            } else {
                                ui_show_error("Error", "Failed to restore NAND");
                            }
                            free(path);
                        }
                        break;
                    }
                    case 2: {
                        char* path = fs_select_directory("Select BOOT0 Dump Location");
                        if (path) {
                            Result rc = system_dump_boot0(path);
                            if (R_SUCCEEDED(rc)) {
                                ui_show_message("Success", "BOOT0 dumped successfully");
                            } else {
                                ui_show_error("Error", "Failed to dump BOOT0");
                            }
                            free(path);
                        }
                        break;
                    }
                    case 3: {
                        char* path = fs_select_directory("Select BOOT1 Dump Location");
                        if (path) {
                            Result rc = system_dump_boot1(path);
                            if (R_SUCCEEDED(rc)) {
                                ui_show_message("Success", "BOOT1 dumped successfully");
                            } else {
                                ui_show_error("Error", "Failed to dump BOOT1");
                            }
                            free(path);
                        }
                        break;
                    }
                }
                break;
            }
            case 3: {
                MenuItem emummc_items[] = {
                    {"Create emuMMC", true},
                    {"Dump emuMMC", true},
                    {"Restore emuMMC", true},
                    {"Back", true}
                };
                int emummc_sel = ui_show_menu("emuMMC Tools", emummc_items, 4);
                switch (emummc_sel) {
                    case 0: {
                        char* path = fs_select_directory("Select emuMMC Location");
                        if (path) {
                            Result rc = emummc_create(path, (u64)29 * 1024ULL * 1024ULL * 1024ULL);
                            if (R_SUCCEEDED(rc)) {
                                ui_show_message("Success", "emuMMC created successfully");
                            } else {
                                ui_show_error("Error", "Failed to create emuMMC");
                            }
                            free(path);
                        }
                        break;
                    }
                    case 1: {
                        char* path = fs_select_directory("Select emuMMC Dump Location");
                        if (path) {
                            Result rc = emummc_dump(path);
                            if (R_SUCCEEDED(rc)) {
                                ui_show_message("Success", "emuMMC dumped successfully");
                            } else {
                                ui_show_error("Error", "Failed to dump emuMMC");
                            }
                            free(path);
                        }
                        break;
                    }
                    case 2: {
                        char* path = fs_select_directory("Select emuMMC Backup");
                        if (path) {
                            Result rc = emummc_restore(path);
                            if (R_SUCCEEDED(rc)) {
                                ui_show_message("Success", "emuMMC restored successfully");
                            } else {
                                ui_show_error("Error", "Failed to restore emuMMC");
                            }
                            free(path);
                        }
                        break;
                    }
                }
                break;
            }
            case 4:
            default:
                return;
        }
    }
}

Result emummc_dump(const char* dump_path) {
    Result rc = 0;
    
    // Initialize transfer buffer
    rc = initialize_transfer_buffer();
    if (R_FAILED(rc)) return rc;
    
    FsDeviceOperator dev_op;
    rc = fsOpenDeviceOperator(&dev_op);
    if (R_FAILED(rc)) return rc;
    
    // Open raw storage
    FsStorage storage;
    rc = fsOpenBisStorage(&storage, FsBisPartitionId_System);
    if (R_FAILED(rc)) {
        fsDeviceOperatorClose(&dev_op);
        return rc;
    }
    
    // Get storage size
        u64 total_size = 0;
        s64 tmp_total = 0;
        rc = fsStorageGetSize(&storage, &tmp_total);
        if (R_SUCCEEDED(rc)) total_size = (u64)tmp_total;
    if (R_FAILED(rc)) {
        fsStorageClose(&storage);
        fsDeviceOperatorClose(&dev_op);
        return rc;
    }
    
    // Create dump file
    char dump_file[PATH_MAX];
    snprintf(dump_file, PATH_MAX, "%s/SYSTEM.img", dump_path);
    
    FILE* out = fopen(dump_file, "wb");
    if (!out) {
        fsStorageClose(&storage);
        fsDeviceOperatorClose(&dev_op);
        return -1;
    }
    
    // Dump in chunks
    u64 offset = 0;
    while (offset < total_size) {
        size_t read_size = (total_size - offset) > BUFFER_SIZE ? 
                          BUFFER_SIZE : (total_size - offset);
        
        rc = fsStorageRead(&storage, offset, transfer_buffer, read_size);
        if (R_FAILED(rc)) break;
        
        if (fwrite(transfer_buffer, 1, read_size, out) != read_size) {
            rc = -2;
            break;
        }
        
        offset += read_size;
    }
    
    // Create emuMMC config
    if (R_SUCCEEDED(rc)) {
        char config_path[PATH_MAX];
        snprintf(config_path, PATH_MAX, "%s/emummc.ini", dump_path);
        
        FILE* config = fopen(config_path, "w");
        if (config) {
            fprintf(config, "[emummc]\n");
            fprintf(config, "enabled=1\n");
            fprintf(config, "sector=0\n");
            fprintf(config, "path=%s\n", dump_file);
            fprintf(config, "nintendo_path=%s/Nintendo\n", dump_path);
            fclose(config);
        }
    }
    
    fclose(out);
    fsStorageClose(&storage);
    fsDeviceOperatorClose(&dev_op);
    return rc;
}

Result emummc_restore(const char* dump_path) {
    Result rc = 0;
    
    // Initialize transfer buffer
    rc = initialize_transfer_buffer();
    if (R_FAILED(rc)) return rc;
    
    FsDeviceOperator dev_op;
    rc = fsOpenDeviceOperator(&dev_op);
    if (R_FAILED(rc)) return rc;
    
    // Open raw storage
    FsStorage storage;
    rc = fsOpenBisStorage(&storage, FsBisPartitionId_System);
    if (R_FAILED(rc)) {
        fsDeviceOperatorClose(&dev_op);
        return rc;
    }
    
    // Open dump file
    char dump_file[PATH_MAX];
    snprintf(dump_file, PATH_MAX, "%s/SYSTEM.img", dump_path);
    
    FILE* in = fopen(dump_file, "rb");
    if (!in) {
        fsStorageClose(&storage);
        fsDeviceOperatorClose(&dev_op);
        return -1;
    }
    
    // Get file size
    fseek(in, 0, SEEK_END);
    u64 file_size = ftell(in);
    fseek(in, 0, SEEK_SET);
    
    // Get storage size
        s64 tmp_total2 = 0;
        u64 storage_size = 0;
        rc = fsStorageGetSize(&storage, &tmp_total2);
        if (R_SUCCEEDED(rc)) storage_size = (u64)tmp_total2;
    if (R_FAILED(rc) || file_size > storage_size) {
        fclose(in);
        fsStorageClose(&storage);
        fsDeviceOperatorClose(&dev_op);
        return -2;
    }
    
    // Restore in chunks
    u64 offset = 0;
    while (offset < file_size) {
        size_t read_size = (file_size - offset) > BUFFER_SIZE ? 
                          BUFFER_SIZE : (file_size - offset);
        
        if (fread(transfer_buffer, 1, read_size, in) != read_size) {
            rc = -3;
            break;
        }
        
        rc = fsStorageWrite(&storage, offset, transfer_buffer, read_size);
        if (R_FAILED(rc)) break;
        
        offset += read_size;
    }
    
    // Flush changes
    if (R_SUCCEEDED(rc)) {
        rc = fsStorageFlush(&storage);
    }
    
    fclose(in);
    fsStorageClose(&storage);
    fsDeviceOperatorClose(&dev_op);
    return rc;
}

Result emummc_create(const char* path, u64 size) {
    Result rc = 0;
    
    // Initialize transfer buffer
    rc = initialize_transfer_buffer();
    if (R_FAILED(rc)) return rc;
    
    // TODO: Implement emuMMC creation
    // 1. Create file/partition of specified size
    // 2. Initialize NAND structure
    // 3. Create emuMMC config
    
    return rc;
}

Result system_dump_nand(const char* dump_path) {
    Result rc = 0;
    
    // Initialize transfer buffer
    rc = initialize_transfer_buffer();
    if (R_FAILED(rc)) return rc;
    
    FsDeviceOperator dev_op;
    rc = fsOpenDeviceOperator(&dev_op);
    if (R_FAILED(rc)) return rc;
    
    // TODO: Implement NAND dump
    // 1. Dump raw NAND
    // 2. Save in chunks
    // 3. Verify integrity
    
    fsDeviceOperatorClose(&dev_op);
    return rc;
}

Result system_restore_nand(const char* dump_path) {
    Result rc = 0;
    
    // Initialize transfer buffer
    rc = initialize_transfer_buffer();
    if (R_FAILED(rc)) return rc;
    
    FsDeviceOperator dev_op;
    rc = fsOpenDeviceOperator(&dev_op);
    if (R_FAILED(rc)) return rc;
    
    // TODO: Implement NAND restore
    // 1. Verify dump integrity
    // 2. Write back to NAND
    // 3. Verify write
    
    fsDeviceOperatorClose(&dev_op);
    return rc;
}

Result system_dump_boot0(const char* dump_path) {
    Result rc = 0;
    
    // Initialize transfer buffer
    rc = initialize_transfer_buffer();
    if (R_FAILED(rc)) return rc;
    
    // TODO: Implement BOOT0 dump
    
    return rc;
}

Result system_dump_boot1(const char* dump_path) {
    Result rc = 0;
    
    // Initialize transfer buffer
    rc = initialize_transfer_buffer();
    if (R_FAILED(rc)) return rc;
    
    // TODO: Implement BOOT1 dump
    
    return rc;
}

Result system_get_info(char* out_info, size_t info_size) {
    Result rc = 0;
    // Provide a lightweight information string without relying on setcal APIs
    // which may not be present or have different types in the installed libnx.
    snprintf(out_info, info_size,
        "Firmware: %u.%u.%u\nHardware: %s\nSerial: %s\n",
        0u, 0u, 0u,
        "Unknown",
        "Unknown"
    );
    return 0;
}

bool system_is_emummc(void) {
    // TODO: Implement emuMMC detection
    return false;
}