#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "ui.h"
#include "ui/ui_data.h"
#include "hb_store.h"
#include "task_queue.h"
#include "core/app.h"
#include "../logger.h"

static UIState ui_state;
static HomebrewApp* apps = NULL;
static size_t app_count = 0;
static bool downloading = false;

void hbstore_ui_init(void) {
    ui_state_init(&ui_state);
    strcpy(ui_state.title, "Homebrew Store");
    strcpy(ui_state.subtitle, "Press Y to refresh  X for help  B to exit");
}

void hbstore_ui_exit(void) {
    if (apps) {
        free(apps);
        apps = NULL;
    }
    app_count = 0;
}

static void refresh_app_list(void) {
    if (apps) {
        free(apps);
        apps = NULL;
    }
    
    Result rc = hbstore_list_apps(&apps, &app_count);
    if (R_FAILED(rc)) {
        app_count = 0;
        return;
    }
    
    // Create menu items
    char** menu_items = malloc(app_count * sizeof(char*));
    if (!menu_items) return;
    
    for (int i = 0; i < app_count; i++) {
        menu_items[i] = malloc(512); // Increase buffer size
        if (menu_items[i]) {
            snprintf(menu_items[i], 512, "%-32.32s v%-10.10s %.32s", apps[i].name, apps[i].version, apps[i].author);
        }
        log_event(LOG_DEBUG, "hbstore: menu item %d = %s", i, menu_items[i] ? menu_items[i] : "(null)");
    }
    
    ui_state_set_menu(&ui_state, (const char**)menu_items, app_count);
    
    // Free menu items
    for (int i = 0; i < app_count; i++) {
        if (menu_items[i]) free(menu_items[i]);
    }
    free(menu_items);
}

void hbstore_ui_update(void) {
    // Ensure any queued tasks make progress while this modal UI is active.
    // The main app normally calls task_queue_process() each frame; when
    // running the hbstore modal loop we must drive the task queue here so
    // downloads and other background tasks do not stall the UI.
    if (!task_queue_is_empty()) task_queue_process();

    MenuAction action = ui_handle_input(&ui_state);
    
    switch (action) {
        case MENU_ACTION_SELECT:
            if (!downloading && ui_state.selected_index < app_count) {
                downloading = true;
                log_event(LOG_INFO, "hbstore: queueing download for %s", apps[ui_state.selected_index].name);
                task_queue_add(TASK_DOWNLOAD_HB, apps[ui_state.selected_index].url, NULL);
            }
            break;
            
        case MENU_ACTION_REFRESH:
            refresh_app_list();
            break;
            
        case MENU_ACTION_BACK:
            app_set_state(APP_STATE_FILE_BROWSER); // Switch back to file browser state
            break;
            
        default:
            break;
    }
    
    // Check download status
    if (downloading) {
        Task* current = task_queue_get_current();
        if (current && current->type == TASK_DOWNLOAD_HB) {
            if (current->status.progress >= 100 || current->status.has_error) {
                downloading = false;
            }
        } else {
            // No current task or different task; clear flag
            downloading = false;
        }
    }
}

void hbstore_ui_render(void) {
    ui_clear_screen();
    ui_render_header(&ui_state);
    
    if (app_count == 0) {
        printf("\nNo homebrew applications found.\nPress Y to refresh the list.\n");
    } else {
        ui_render_menu(&ui_state);
        
        // Show app details
        if (ui_state.selected_index < app_count) {
            printf("\n\x1b[7m Details \x1b[0m\n");
            printf("Name: %s\n", apps[ui_state.selected_index].name);
            printf("Version: %s\n", apps[ui_state.selected_index].version);
            printf("Author: %s\n", apps[ui_state.selected_index].author);
            printf("Description: %s\n", apps[ui_state.selected_index].description);
        }
    }
    
    if (downloading) {
        Task* current = task_queue_get_current();
        if (current && current->type == TASK_DOWNLOAD_HB) {
            ui_render_progress("Downloading...", current->status.progress);
            if (current->status.has_error) {
                ui_render_error(current->status.error_msg);
            }
        }
    }
    
    ui_render_help(&ui_state);
    ui_refresh();
}