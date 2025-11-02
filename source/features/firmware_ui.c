#include "firmware_ui.h"
#include "../firmware_manager.h"
#include "../ui.h"
#include "../dialog.h"
#include "../fs.h"
#include "../task_queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void _show_firmware_info(void) {
    FirmwareInfo info;
    Result rc = firmware_get_version(&info);
    
    if (R_SUCCEEDED(rc)) {
        ui_show_message(
            "Firmware Information",
            "Version: %s\n"
            "Major: %u\n"
            "Minor: %u\n"
            "Micro: %u",
            info.version_string,
            info.version_major,
            info.version_minor,
            info.version_micro
        );
    } else {
        ui_show_error("Failed to get firmware version", firmware_get_error_msg(rc));
    }
}

static void _export_progress_callback(size_t current, size_t total) {
    static char progress_text[64];
    snprintf(progress_text, sizeof(progress_text), 
             "Exporting firmware: %.1f%%", (float)current / total * 100.0f);
    ui_set_status(progress_text);
}

static void _export_firmware_task(void* arg) {
    char* output_path = (char*)arg;
    bool include_exfat = true; // Allow configuring this via UI later

    Result rc = firmware_export(output_path, include_exfat, _export_progress_callback);
    
    if (R_SUCCEEDED(rc)) {
        ui_show_message("Success", "Firmware exported to:\n%s", output_path);
    } else {
        ui_show_error("Export Failed", firmware_get_error_msg(rc));
    }

    free(output_path);
}

static void _start_firmware_export(void) {
    char* output_path = fs_select_directory("Select Export Location");
    if (!output_path) return;

    char* task_path = strdup(output_path);
    free(output_path);

    if (!task_path) {
        ui_show_error("Error", "Out of memory");
        return;
    }

    if (!task_queue_add("Exporting Firmware", _export_firmware_task, task_path)) {
        free(task_path);
        ui_show_error("Error", "Failed to start export task");
    }
}

static void _extract_content(void) {
    char** content_paths = NULL;
    size_t count = 0;
    
    Result rc = firmware_list_contents(&content_paths, &count);
    if (R_FAILED(rc)) {
        ui_show_error("Error", "Failed to list firmware contents");
        return;
    }

    // Create menu items for each content
    MenuItem* items = calloc(count + 1, sizeof(MenuItem));
    if (!items) {
        firmware_free_content_list(content_paths, count);
        ui_show_error("Error", "Out of memory");
        return;
    }

    for (size_t i = 0; i < count; i++) {
        items[i].text = content_paths[i];
        items[i].enabled = true;
    }

    int selected = ui_show_menu("Select Content to Extract", items, count);
    if (selected >= 0) {
        char* output_path = fs_save_file_picker("Select Output Location",
                                              content_paths[selected]);
        if (output_path) {
            rc = firmware_extract_file(content_paths[selected], output_path);
            if (R_SUCCEEDED(rc)) {
                ui_show_message("Success", "Content extracted to:\n%s", output_path);
            } else {
                ui_show_error("Extract Failed", firmware_get_error_msg(rc));
            }
            free(output_path);
        }
    }

    // Clean up
    free(items);
    firmware_free_content_list(content_paths, count);
}

static void _verify_firmware_package(void) {
    char* package_path = fs_select_directory("Select Firmware Package");
    if (!package_path) return;

    FirmwareInfo info;
    Result rc = firmware_verify_package(package_path, &info);
    
    if (R_SUCCEEDED(rc)) {
        ui_show_message(
            "Package Information",
            "Version: %s\n"
            "Size: %.2f MB\n"
            "ExFAT Support: %s",
            info.version_string,
            (float)info.package_size / (1024.0f * 1024.0f),
            info.is_exfat ? "Yes" : "No"
        );
    } else {
        ui_show_error("Verification Failed", firmware_get_error_msg(rc));
    }

    free(package_path);
}

Result firmware_ui_init(void) {
    return firmware_init();
}

void firmware_ui_show_menu(void) {
    MenuItem items[] = {
        {"View Current Firmware Info", true},
        {"Export Firmware Package", true},
        {"Extract Specific Content", true},
        {"Verify Firmware Package", true},
        {"Back", true}
    };

    while (1) {
        int selection = ui_show_menu("Firmware Management", items, 5);
        
        switch (selection) {
            case 0:
                _show_firmware_info();
                break;
            case 1:
                _start_firmware_export();
                break;
            case 2:
                _extract_content();
                break;
            case 3:
                _verify_firmware_package();
                break;
            case 4:
            default:
                return;
        }
    }
}