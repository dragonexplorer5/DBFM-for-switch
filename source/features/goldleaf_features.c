#include "goldleaf_features.h"
#include "../verify.h"
#include "../usb_service.h"
#include "../title_key.h"
#include "../browser.h"
#include "../ui.h"
#include "../task_queue.h"
#include "../fs.h"
#include <stdio.h>
#include <string.h>

static void _show_transfer_progress(size_t current, size_t total) {
    static char progress_text[64];
    float percentage = (float)current / total * 100.0f;
    snprintf(progress_text, sizeof(progress_text),
             "Transferring: %.1f%%", percentage);
    ui_set_status(progress_text);
}

static void _start_usb_service(void) {
    Result rc = usb_start_service();
    if (R_SUCCEEDED(rc)) {
        ui_show_message("USB Service", "USB service started successfully.\n"
                       "Current state: %s", usb_get_state_string(usb_get_state()));
    } else {
        ui_show_error("USB Error", "Failed to start USB service: %s",
                     usb_get_error_message(rc));
    }
}

static void _verify_nsp_file(void) {
    char* path = fs_open_file_picker("Select NSP file", "NSP files (*.nsp)");
    if (!path) return;

    NspVerifyResult verify_result;
    Result rc = verify_nsp_file(path, &verify_result);

    if (R_SUCCEEDED(rc)) {
        char info[1024];
        snprintf(info, sizeof(info),
                "Title ID: %016lx\n"
                "Title Name: %s\n"
                "Contains:\n"
                "- Program NCA: %s\n"
                "- Control NCA: %s\n"
                "- Legal NCA: %s\n"
                "- Meta NCA: %s\n"
                "Total NCAs: %zu\n"
                "Minimum Key Generation: %u\n"
                "Ticket Required: %s\n"
                "Ticket Present: %s",
                verify_result.title_id,
                verify_result.title_name,
                verify_result.has_program ? "Yes" : "No",
                verify_result.has_control ? "Yes" : "No",
                verify_result.has_legal ? "Yes" : "No",
                verify_result.has_meta ? "Yes" : "No",
                verify_result.nca_count,
                verify_result.min_key_gen,
                verify_result.requires_ticket ? "Yes" : "No",
                verify_result.has_ticket ? "Yes" : "No");

        ui_show_message("NSP Verification", info);
    } else {
        ui_show_error("Verification Error", verify_get_error_message(rc));
    }

    verify_free_nsp_result(&verify_result);
    free(path);
}

static void _manage_title_keys(void) {
    TitleKeyInfo* keys;
    size_t count;
    Result rc = titlekey_list(&keys, &count);

    if (R_SUCCEEDED(rc)) {
        if (count == 0) {
            ui_show_message("Title Keys", "No title keys found.");
            return;
        }

        MenuItem* items = calloc(count + 1, sizeof(MenuItem));
        if (!items) {
            titlekey_free_list(keys);
            ui_show_error("Error", "Out of memory");
            return;
        }

        for (size_t i = 0; i < count; i++) {
            char* text = malloc(64);
            if (text) {
                snprintf(text, 64, "%016lx", keys[i].title_id);
                items[i].text = text;
                items[i].enabled = true;
            }
        }
        items[count].text = "Back";
        items[count].enabled = true;

        int selection = ui_show_menu("Title Key Management", items, count + 1);
        if (selection >= 0 && selection < count) {
            MenuItem key_options[] = {
                {"Export Key", true},
                {"Remove Key", true},
                {"Back", true}
            };

            int key_action = ui_show_menu("Key Options", key_options, 3);
            switch (key_action) {
                case 0: {
                    char* path = fs_save_file_picker("Save Title Key", "key");
                    if (path) {
                        u8 key[16];
                        rc = titlekey_export(keys[selection].title_id, key);
                        if (R_SUCCEEDED(rc)) {
                            FILE* f = fopen(path, "wb");
                            if (f) {
                                fwrite(key, 1, 16, f);
                                fclose(f);
                                ui_show_message("Success", "Title key exported successfully");
                            } else {
                                ui_show_error("Error", "Failed to save key file");
                            }
                        } else {
                            ui_show_error("Error", titlekey_get_error(rc));
                        }
                        free(path);
                    }
                    break;
                }
                case 1: {
                    if (ui_show_dialog("Confirm", "Remove this title key?")) {
                        rc = titlekey_remove(keys[selection].title_id);
                        if (R_SUCCEEDED(rc)) {
                            ui_show_message("Success", "Title key removed");
                        } else {
                            ui_show_error("Error", titlekey_get_error(rc));
                        }
                    }
                    break;
                }
            }
        }

        for (size_t i = 0; i < count; i++) {
            free((char*)items[i].text);
        }
        free(items);
        titlekey_free_list(keys);
    } else {
        ui_show_error("Error", titlekey_get_error(rc));
    }
}

static void _import_title_key(void) {
    char* path = fs_open_file_picker("Select ticket file", "Ticket files (*.tik)");
    if (!path) return;

    FILE* f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        size_t size = ftell(f);
        fseek(f, 0, SEEK_SET);

        void* data = malloc(size);
        if (data) {
            if (fread(data, 1, size, f) == size) {
                Result rc = titlekey_import(data, size);
                if (R_SUCCEEDED(rc)) {
                    ui_show_message("Success", "Title key imported successfully");
                } else {
                    ui_show_error("Error", titlekey_get_error(rc));
                }
            }
            free(data);
        }
        fclose(f);
    }
    free(path);
}

static void _browse_url(void) {
    char url[1024] = "";
    if (ui_show_keyboard("Enter URL", url, sizeof(url))) {
        if (strlen(url) > 0) {
            Result rc = browser_open_url(url);
            if (R_FAILED(rc)) {
                ui_show_error("Browser Error", browser_get_error(rc));
            }
        }
    }
}

Result goldleaf_init(void) {
    Result rc = verify_init();
    if (R_FAILED(rc)) return rc;

    rc = usb_init();
    if (R_FAILED(rc)) return rc;

    rc = titlekey_init();
    if (R_FAILED(rc)) return rc;

    rc = browser_init();
    if (R_FAILED(rc)) return rc;

    return 0;
}

void goldleaf_exit(void) {
    browser_exit();
    titlekey_exit();
    usb_exit();
    verify_exit();
}

void goldleaf_show_menu(void) {
    MenuItem items[] = {
        {"USB Connection", true},
        {"Verify NSP/NCA", true},
        {"Title Key Management", true},
        {"Import Title Key", true},
        {"Web Browser", true},
        {"Back", true}
    };

    while (1) {
        int selection = ui_show_menu("Goldleaf Features", items, 6);
        
        switch (selection) {
            case 0:
                _start_usb_service();
                break;
            case 1:
                _verify_nsp_file();
                break;
            case 2:
                _manage_title_keys();
                break;
            case 3:
                _import_title_key();
                break;
            case 4:
                _browse_url();
                break;
            case 5:
            default:
                return;
        }
    }
}