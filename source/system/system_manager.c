#include <string.h>
#include <malloc.h>
#include "system_manager.h"
#include "ui.h"
#include "features/firmware_ui.h"

#define SECTOR_SIZE 0x200
#define BUFFER_SIZE (8 * 1024 * 1024)

static u8* transfer_buffer = NULL;

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
                            Result rc = emummc_create(path, 29_GiB);
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
    rc = fsStorageGetTotalSize(&storage, &total_size);
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
    u64 storage_size = 0;
    rc = fsStorageGetTotalSize(&storage, &storage_size);
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
    SetSysFirmwareVersion fw;
    SetCalVersion cal;
    
    rc = setsysGetFirmwareVersion(&fw);
    if (R_FAILED(rc)) return rc;
    
    rc = setcalGetVersionHash(&cal);
    if (R_FAILED(rc)) return rc;
    
    snprintf(out_info, info_size,
        "Firmware: %u.%u.%u-%u\n"
        "Hardware: %s\n"
        "Serial: %s\n"
        "Cal0 Version: %lu",
        fw.major, fw.minor, fw.micro, fw.revision_major,
        "TODO", // TODO: Get actual hardware info
        "TODO", // TODO: Get serial number
        cal.version
    );
    
    return rc;
}

bool system_is_emummc(void) {
    // TODO: Implement emuMMC detection
    return false;
}