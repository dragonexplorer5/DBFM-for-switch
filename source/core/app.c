#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <switch.h>
#include "app.h"
#include "ui.h"
#include "file_explorer.h"
#include "save_manager.h"
#include "nsp_manager.h"
#include "system_manager.h"
#include "hb_store.h"
#include "../util/install.h"
#include "task_queue.h"
#include "secure.h"
#include "goldleaf_features.h"
#include "settings.h"
#include "../logger.h"
#include "../ui/ui_data.h"

static AppState current_state = APP_STATE_FILE_BROWSER;
static bool running = true;
static PadState pad;
static bool g_romfs_inited = false;
static bool g_socket_inited = false;
static bool g_nifm_inited = false;
static UIState g_ui_state;
// shared terminal view size (probed at startup)
static int g_view_rows = 20;
static int g_view_cols = 80;

// Append a small timestamped message to sdmc:/dbfm/logs/init_debug.txt so
// maintainers can collect init failure traces from a device without a
// console capture. Directory creation is best-effort.
static void write_init_log(const char *fmt, ...) {
    // Ensure directories exist (ignore errors)
    mkdir("sdmc:/dbfm", 0777);
    mkdir("sdmc:/dbfm/logs", 0777);
    FILE *f = fopen("sdmc:/dbfm/logs/init_debug.txt", "a");
    if (!f) return;
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    if (lt) {
        char ts[64];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", lt);
        fprintf(f, "%s - ", ts);
    }
    va_list ap; va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\n");
    fclose(f);
}

Result app_init(void) {
    Result rc = 0;
    
    // Initialize input
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    
    // Initialize services
    printf("app_init: romfsInit()\n"); write_init_log("app_init: romfsInit()");
    rc = romfsInit();
    if (R_FAILED(rc)) {
        printf("app_init: romfsInit failed (non-fatal): 0x%x\n", rc);
        write_init_log("romfsInit failed (non-fatal): 0x%x", rc);
        g_romfs_inited = false;
    } else {
        printf("app_init: romfsInit OK\n"); write_init_log("romfsInit OK");
        g_romfs_inited = true;
    }

    printf("app_init: socketInitializeDefault()\n"); write_init_log("app_init: socketInitializeDefault()");
    rc = socketInitializeDefault();
    if (R_FAILED(rc)) {
        printf("app_init: socketInitializeDefault failed (network disabled): 0x%x\n", rc);
        write_init_log("socketInitializeDefault failed (network disabled): 0x%x", rc);
        g_socket_inited = false;
    } else {
        printf("app_init: socketInitializeDefault OK\n"); write_init_log("socketInitializeDefault OK");
        g_socket_inited = true;
    }

    if (g_socket_inited) {
        printf("app_init: nifmInitialize()\n"); write_init_log("app_init: nifmInitialize()");
        rc = nifmInitialize(NifmServiceType_User);
        if (R_FAILED(rc)) {
            printf("app_init: nifmInitialize failed (network features limited): 0x%x\n", rc);
            write_init_log("nifmInitialize failed (network features limited): 0x%x", rc);
            g_nifm_inited = false;
        } else {
            printf("app_init: nifmInitialize OK\n"); write_init_log("nifmInitialize OK");
            g_nifm_inited = true;
        }
    } else {
        write_init_log("Skipping nifmInitialize because socketInitializeDefault failed");
    }

    // Initialize security system first
    printf("app_init: secure_init()\n"); write_init_log("app_init: secure_init()");
    rc = secure_init();
    if (R_FAILED(rc)) { printf("app_init: secure_init failed: 0x%x\n", rc); write_init_log("secure_init failed: 0x%x", rc); return rc; }
    printf("app_init: secure_init OK\n"); write_init_log("secure_init OK");

    // Initialize subsystems
    printf("app_init: task_queue_init()\n"); write_init_log("app_init: task_queue_init()");
    task_queue_init();
    printf("app_init: task_queue_init OK\n"); write_init_log("task_queue_init OK");

    printf("app_init: hbstore_init()\n"); write_init_log("app_init: hbstore_init()");
    rc = hbstore_init();
    if (R_FAILED(rc)) { printf("app_init: hbstore_init failed: 0x%x\n", rc); write_init_log("hbstore_init failed: 0x%x", rc); return rc; }
    printf("app_init: hbstore_init OK\n"); write_init_log("hbstore_init OK");

    printf("app_init: system_manager_init()\n"); write_init_log("app_init: system_manager_init()");
    rc = system_manager_init();
    if (R_FAILED(rc)) { printf("app_init: system_manager_init failed: 0x%x\n", rc); write_init_log("system_manager_init failed: 0x%x", rc); return rc; }
    printf("app_init: system_manager_init OK\n"); write_init_log("system_manager_init OK");

    printf("app_init: logger_init()\n"); write_init_log("app_init: logger_init()");
    rc = logger_init();
    if (R_FAILED(rc)) { printf("app_init: logger_init failed: 0x%x\n", rc); write_init_log("logger_init failed: 0x%x", rc); return rc; }
    printf("app_init: logger_init OK\n"); write_init_log("logger_init OK");
    log_event(LOG_INFO, "DBFM Started", "Application initialization complete");

    printf("app_init: goldleaf_init()\n"); write_init_log("app_init: goldleaf_init()");
    rc = goldleaf_init();
    if (R_FAILED(rc)) { printf("app_init: goldleaf_init failed: 0x%x\n", rc); write_init_log("goldleaf_init failed: 0x%x", rc); return rc; }
    printf("app_init: goldleaf_init OK\n"); write_init_log("goldleaf_init OK");

    /* Initialize UI state for homescreen/menu rendering */
    ui_state_init(&g_ui_state);
    ui_state_set_menu(&g_ui_state, g_menu_items, g_menu_count);
    // Probe terminal size so layouts match the device
    int pr = 0, pc = 0;
    if (ui_probe_terminal_size(&pr, &pc)) {
        if (pr > 0) g_view_rows = pr;
        if (pc > 0) g_view_cols = pc;
    }
    // Load favorites from SD
    ui_favorites_load();

    return rc;
}

void app_exit(void) {
    // Clean up subsystems
    hbstore_exit();
    task_queue_clear();
    system_manager_exit();
    goldleaf_exit();

    // Clean up security system last
    secure_exit();

    // Clean up services that were initialized
    if (g_nifm_inited) nifmExit();
    if (g_socket_inited) socketExit();
    if (g_romfs_inited) romfsExit();
}

void app_set_state(AppState new_state) {
    current_state = new_state;
}

AppState app_get_state(void) {
    return current_state;
}

void app_process_input(void) {
    // Get input state
    padUpdate(&pad);
    u64 kDown = padGetButtonsDown(&pad);
    
    // Global input handling
    // '+' opens settings (mapped from user request). Use conservative view dims.
    if (kDown & HidNpadButton_Plus) {
        settings_menu(g_view_rows, g_view_cols);
        return;
    }
    
    // State-specific input handling
    switch (current_state) {
        case APP_STATE_FILE_BROWSER:
            {
                // Let the UI subsystem process navigation input (updates selected_index)
                MenuAction act = ui_handle_input(&g_ui_state);

                // Read analog sticks/buttons for extra navigation/actions
                u64 kHeld = padGetButtons(&pad);
                HidAnalogStickState lStick = padGetStickPos(&pad, 0);

                // Left stick vertical navigation (deadzone) - move selection up/down
                if (abs(lStick.y) > 0x4000) {
                    if (lStick.y > 0 && g_ui_state.selected_index < g_ui_state.menu_item_count - 1) g_ui_state.selected_index++;
                    else if (lStick.y < 0 && g_ui_state.selected_index > 0) g_ui_state.selected_index--;
                }

                // Quick scroll with triggers
                if (kDown & HidNpadButton_ZL) {
                    g_ui_state.selected_index -= 3; if (g_ui_state.selected_index < 0) g_ui_state.selected_index = 0;
                }
                if (kDown & HidNpadButton_ZR) {
                    g_ui_state.selected_index += 3; if (g_ui_state.selected_index >= g_ui_state.menu_item_count) g_ui_state.selected_index = g_ui_state.menu_item_count - 1;
                }

                // Cycle tabs with L / R
                if (kDown & HidNpadButton_L) {
                    AppState tabs[] = { APP_STATE_FILE_BROWSER, APP_STATE_NSP_MANAGER, APP_STATE_SYSTEM_TOOLS };
                    int tidx = 0; for (int i = 0; i < 3; ++i) if (current_state == tabs[i]) { tidx = i; break; }
                    tidx = (tidx - 1 + 3) % 3; current_state = tabs[tidx];
                }
                if (kDown & HidNpadButton_R) {
                    AppState tabs[] = { APP_STATE_FILE_BROWSER, APP_STATE_NSP_MANAGER, APP_STATE_SYSTEM_TOOLS };
                    int tidx = 0; for (int i = 0; i < 3; ++i) if (current_state == tabs[i]) { tidx = i; break; }
                    tidx = (tidx + 1) % 3; current_state = tabs[tidx];
                }

                // Open logs with '-'
                if (kDown & HidNpadButton_Minus) {
                    // Open logs directory in file explorer
                    file_explorer_open("sdmc:/dbfm/logs/", g_view_rows, g_view_cols);
                }

                // Context menu (X) and quick action (Y)
                if (kDown & HidNpadButton_X) {
                    // Offer context options for the currently selected action
                    const char *ctx_items[] = { "Properties", "Rename", "Delete", "Cancel" };
                    (void)ctx_items;
                    int sel = ui_show_menu("Context", (MenuItem []){{"Properties",true},{"Rename",true},{"Delete",true},{"Cancel",true}}, 4);
                    (void)sel; // placeholder - real implementation would branch per item
                    write_init_log("ui: context menu selected=%d for idx=%d", sel, g_ui_state.selected_index);
                }
                if (kDown & HidNpadButton_Y) {
                    // Quick action: toggle favorite for selected
                    write_init_log("ui: toggle favorite idx=%d", g_ui_state.selected_index);
                    ui_show_message("Favorites", "Toggled favorite (idx=%d)", g_ui_state.selected_index);
                }

                // Handle select/confirm
                if ((act == MENU_ACTION_SELECT) || (kDown & HidNpadButton_A)) {
                    int sel = g_ui_state.selected_index;
                    write_init_log("ui: select index=%d", sel);
                    switch (sel) {
                        case 0: // File Manager
                            file_explorer_open("/", g_view_rows, g_view_cols);
                            break;
                        case 1: // Game Install/Download
                            scan_installs(g_candidates, g_candidate_count);
                            show_install_list(g_view_rows, g_candidates, g_candidate_count, 0);
                            break;
                        case 2: // Homebrew Store
                            {
                                // Modal hbstore UI (blocking loop)
                                extern void hbstore_ui_init(void);
                                extern void hbstore_ui_exit(void);
                                extern void hbstore_ui_update(void);
                                extern void hbstore_ui_render(void);
                                hbstore_ui_init();
                                while (appletMainLoop()) {
                                    hbstore_ui_update();
                                    hbstore_ui_render();
                                    // Exit hbstore UI when app state returns to browser
                                    if (app_get_state() != APP_STATE_FILE_BROWSER) break;
                                    svcSleepThread(16666666ULL);
                                }
                                hbstore_ui_exit();
                            }
                            break;
                        case 3: // Save Manager
                            {
                                // Provide a minimal save manager list view using save_list_titles
                                Result r; char **titles = NULL; int tc = 0;
                                r = save_list_titles(&titles, &tc);
                                if (R_SUCCEEDED(r) && tc > 0) {
                                    // Build a tiny MenuItem array for ui_show_menu
                                    MenuItem *items = calloc(tc + 1, sizeof(MenuItem));
                                    for (int i = 0; i < tc; ++i) { items[i].text = titles[i]; items[i].enabled = true; }
                                    items[tc].text = "Back"; items[tc].enabled = true;
                                    int sel_idx = ui_show_menu("Save Manager", items, tc + 1);
                                    free(items);
                                    for (int i = 0; i < tc; ++i) { if (titles[i]) free(titles[i]); }
                                    free(titles);
                                    (void)sel_idx; // placeholder: future work to backup/restore selection
                                } else {
                                    ui_show_message("Save Manager", "No saves found or error reading saves.");
                                }
                            }
                            break;
                        case 4: // System Tools
                            system_manager_show_menu();
                            break;
                        case 5: // Settings
                            settings_menu(g_view_rows, g_view_cols);
                            break;
                        case 6: // Search
                            // For now reuse file explorer for search entry point (SD root)
                            file_explorer_open("/", g_view_rows, g_view_cols);
                            break;
                        case 7: // Downloads Queue
                            ui_show_downloads_queue(g_view_rows, g_view_cols);
                            break;
                        case 8: // Logs
                            file_explorer_open("sdmc:/dbfm/logs/", g_view_rows, g_view_cols);
                            break;
                        case 9: // Themes
                            // Use ui_show_menu to pick a theme
                            {
                                extern const char *g_theme_lines[]; extern const int g_theme_count;
                                MenuItem *items = calloc(g_theme_count + 1, sizeof(MenuItem));
                                for (int i = 0; i < g_theme_count; ++i) { items[i].text = g_theme_lines[i]; items[i].enabled = true; }
                                items[g_theme_count].text = "Back"; items[g_theme_count].enabled = true;
                                int choice = ui_show_menu("Themes", items, g_theme_count + 1);
                                free(items);
                                (void)choice; // TODO: hook theme application
                            }
                            break;
                        case 10: // News/Updates
                            ui_show_message("News", "No news source configured.");
                            break;
                        case 11: // Favorites
                            {
                                ui_favorites_load();
                                int fc = ui_favorites_count();
                                if (fc > 0) {
                                    MenuItem *items = calloc(fc + 1, sizeof(MenuItem));
                                    for (int i = 0; i < fc; ++i) { items[i].text = ui_favorites_get(i); items[i].enabled = true; }
                                    items[fc].text = "Back"; items[fc].enabled = true;
                                    int fsel = ui_show_menu("Favorites", items, fc + 1);
                                    free(items);
                                    (void)fsel;
                                } else {
                                    ui_show_message("Favorites", "No favorites saved.");
                                }
                            }
                            break;
                        default:
                            ui_show_message("Action", "Unknown selection %d", sel);
                            break;
                    }
                }

                // Back button: if in a substate, return to homescreen. Here we are already in homescreen
                if (kDown & HidNpadButton_B) {
                    // No-op on homescreen; elsewhere code will handle B during blocking module runs
                }
            }
            break;
            
        case APP_STATE_SAVE_MANAGER:
            // TODO: Implement save manager input handling
            break;
            
        case APP_STATE_NSP_MANAGER:
            // TODO: Implement NSP manager input handling
            break;
            
        case APP_STATE_SYSTEM_TOOLS:
            // TODO: Implement system tools input handling
            break;
            
        case APP_STATE_HB_STORE:
            // TODO: Implement homebrew store input handling
            break;
            
        case APP_STATE_SETTINGS:
            // TODO: Implement settings input handling
            break;
            
        case APP_STATE_TASK_QUEUE:
            // TODO: Implement task queue input handling
            break;
            
        case APP_STATE_FIRMWARE_MANAGER:
            // TODO: Implement firmware manager input handling
            break;
            
        case APP_STATE_GOLDLEAF_FEATURES:
            // TODO: Implement goldleaf features input handling
            break;
            
        case APP_STATE_SECURITY_AUDIT:
            // TODO: Implement security audit input handling
            break;
            
        case APP_STATE_SECURITY_SETTINGS:
            // TODO: Implement security settings input handling
            break;
    }
}

void app_update(void) {
    // Process task queue
    if (!task_queue_is_empty()) {
        task_queue_process();
    }
    // Check auto-mode triggers (battery/storage) and apply modes if needed
    settings_check_auto_mode();
    
    // State-specific updates
    switch (current_state) {
        case APP_STATE_FILE_BROWSER:
            // TODO: Update file browser state
            break;
            
        case APP_STATE_SAVE_MANAGER:
            // TODO: Update save manager state
            break;
            
        case APP_STATE_NSP_MANAGER:
            // TODO: Update NSP manager state
            break;
            
        case APP_STATE_SYSTEM_TOOLS:
            // TODO: Update system tools state
            break;
            
        case APP_STATE_HB_STORE:
            // TODO: Update homebrew store state
            break;
            
        case APP_STATE_SETTINGS:
            // TODO: Update settings state
            break;
            
        case APP_STATE_TASK_QUEUE:
            // TODO: Update task queue state
            break;
            
        case APP_STATE_FIRMWARE_MANAGER:
            // TODO: Update firmware manager state
            break;
            
        case APP_STATE_GOLDLEAF_FEATURES:
            // TODO: Update goldleaf features state
            break;
            
        case APP_STATE_SECURITY_AUDIT:
            // TODO: Update security audit state
            break;
            
        case APP_STATE_SECURITY_SETTINGS:
            // TODO: Update security settings state
            break;
    }
}

void app_render(void) {
    // Clear screen first
    consoleClear();

    /*
     * Minimal rendering strategy:
     * - Map the legacy "main menu" to a visible homescreen on boot.
     * - The project uses both AppState and AppPage concepts; the
     *   homescreen renderer lives in the UI layer and expects AppPage.
     * - Use conservative terminal dimensions so layout produces visible
     *   text/shapes on hardware even without a terminal probe.
     */
    int top_row = 0;
    int selected_row = g_ui_state.selected_index;
    int view_rows = g_view_rows;   // use probed/stored values
    int view_cols = g_view_cols;   // use probed/stored values

    switch (current_state) {
        case APP_STATE_FILE_BROWSER:
            // Show the main homescreen when the app first boots and update
            // it each frame using the UI state's selected index.
            render_active_view(top_row, selected_row, PAGE_MAIN_MENU, NULL, 0, view_rows, view_cols);
            break;

        case APP_STATE_SAVE_MANAGER:
            // TODO: Render save manager UI
            break;

        case APP_STATE_NSP_MANAGER:
            // TODO: Render NSP manager UI
            break;

        case APP_STATE_SYSTEM_TOOLS:
            // TODO: Render system tools UI
            break;

        case APP_STATE_HB_STORE:
            // TODO: Render homebrew store UI
            break;

        case APP_STATE_SETTINGS:
            // TODO: Render settings UI
            break;

        case APP_STATE_TASK_QUEUE:
            // TODO: Render task queue UI
            break;

        case APP_STATE_FIRMWARE_MANAGER:
            // TODO: Render firmware manager UI
            break;

        case APP_STATE_GOLDLEAF_FEATURES:
            // TODO: Render goldleaf features UI
            break;

        case APP_STATE_SECURITY_AUDIT:
            // TODO: Render security audit UI
            break;

        case APP_STATE_SECURITY_SETTINGS:
            // TODO: Render security settings UI
            break;
    }

    /* Force a console update so the first frame is visible on boot */
    consoleUpdate(NULL);
}

void app_run(void) {
    /* Target frame interval: ~33.333ms (1/30s) => 33,333,333 ns */
    const unsigned long long target_frame_ns = 33333333ULL;
    struct timespec last; clock_gettime(CLOCK_MONOTONIC, &last);

    while (running) {
        struct timespec frame_start; clock_gettime(CLOCK_MONOTONIC, &frame_start);

        app_process_input();
        app_update();
        app_render();

        /* Compute elapsed time for this frame in nanoseconds */
        struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
        unsigned long long elapsed_ns = (now.tv_sec - frame_start.tv_sec) * 1000000000ULL + (now.tv_nsec - frame_start.tv_nsec);

        if (elapsed_ns < target_frame_ns) {
            unsigned long long sleep_ns = target_frame_ns - elapsed_ns;
            /* svcSleepThread accepts nanoseconds */
            svcSleepThread(sleep_ns);
        } else {
            /* If frame took longer than target, yield briefly to avoid busy-loop */
            svcSleepThread(1000000ull); // 1ms
        }
    }
}