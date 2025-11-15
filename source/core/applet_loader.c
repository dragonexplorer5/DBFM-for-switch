#include "applet_loader.h"
#include "../security/crypto.h"
#include "../security/secure_validation.h"
#include "../browser.h"
#include "../save/save_manager.h"
#include <malloc.h>
#include <string.h>

// Memory management constants
#define APPLET_MIN_MEMORY (4 * 1024 * 1024)    // 4MB minimum
#define APPLET_MAX_MEMORY (512 * 1024 * 1024)  // 512MB maximum
#define APPLET_DEFAULT_STACK (1 * 1024 * 1024) // 1MB stack
#define SYSTEM_RESERVED_MEMORY (128 * 1024 * 1024) // 128MB reserved

// Static applet registry
static struct {
    CustomAppletInstance* loaded_applets[16];
    size_t loaded_count;
    size_t total_memory_used;
    bool initialized;
} g_applet_system = {0};

// Default applet configurations
static const CustomAppletInfo g_default_configs[] = {
    {
        .type = CUSTOM_APPLET_BROWSER,
        .name = "Browser",
        .description = "Hidden browser access",
        .memory_req = {
            .min_memory = 64 * 1024 * 1024,
            .preferred_memory = 128 * 1024 * 1024,
            .requires_gpu = true,
            .stack_size = APPLET_DEFAULT_STACK,
            .heap_size = 32 * 1024 * 1024
        },
        .version = "1.0.0",
        .auto_suspend = true,
        .preserve_state = true
    },
    // Add other applet configs here
};

Result applet_loader_init(void) {
    if (g_applet_system.initialized) {
        return 0;
    }

    memset(&g_applet_system, 0, sizeof(g_applet_system));
    g_applet_system.initialized = true;
    return 0;
}

void applet_loader_exit(void) {
    if (!g_applet_system.initialized) {
        return;
    }

    // Unload all applets
    for (size_t i = 0; i < g_applet_system.loaded_count; i++) {
        if (g_applet_system.loaded_applets[i]) {
            applet_unload(g_applet_system.loaded_applets[i]);
        }
    }

    memset(&g_applet_system, 0, sizeof(g_applet_system));
}

Result applet_load(CustomAppletType type, CustomAppletInstance** instance) {
    Result rc = 0;
    CustomAppletInstance* new_instance = NULL;
    bool memory_available = false;

    // Validate parameters
    if (!instance || type < 0 || type >= CUSTOM_APPLET_SECURITY) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    // Check if already loaded
    for (size_t i = 0; i < g_applet_system.loaded_count; i++) {
        if (g_applet_system.loaded_applets[i] && 
            g_applet_system.loaded_applets[i]->info.type == type) {
            *instance = g_applet_system.loaded_applets[i];
            return 0;
        }
    }

    // Check system resources
    CustomAppletInfo config = g_default_configs[type];
    rc = applet_check_memory_available(&config.memory_req, &memory_available);
    if (R_FAILED(rc) || !memory_available) {
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }


    // Allocate instance
    new_instance = (CustomAppletInstance*)malloc(sizeof(CustomAppletInstance));
    if (!new_instance) {
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }
    memset(new_instance, 0, sizeof(CustomAppletInstance));

    // Set up instance
    new_instance->info = config;
    new_instance->state = CUSTOM_APPLET_STATE_LOADING;

    // Allocate memory
    rc = applet_allocate_memory(new_instance);
    if (R_FAILED(rc)) {
        free(new_instance);
        return rc;
    }

    // Load applet-specific resources
    switch (type) {
        case CUSTOM_APPLET_BROWSER:
            rc = browser_init();
            break;
        case CUSTOM_APPLET_SAVE_MANAGER:
            rc = save_manager_init();
            break;
        // Add other applet initializations
        default:
            rc = 0;
            break;
    }

    if (R_FAILED(rc)) {
        applet_free_memory(new_instance);
        free(new_instance);
        return rc;
    }

    // Add to registry
    if (g_applet_system.loaded_count < 16) {
        g_applet_system.loaded_applets[g_applet_system.loaded_count++] = new_instance;
        g_applet_system.total_memory_used += new_instance->memory_size;
    } else {
        applet_free_memory(new_instance);
        free(new_instance);
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }

    new_instance->state = CUSTOM_APPLET_STATE_READY;
    *instance = new_instance;
    return 0;
}

Result applet_unload(CustomAppletInstance* instance) {
    if (!instance) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    // Save state if needed
    if (instance->info.preserve_state) {
        applet_save_state(instance);
    }

    // Cleanup applet-specific resources
    switch (instance->info.type) {
        case CUSTOM_APPLET_BROWSER:
            browser_exit();
            break;
        case CUSTOM_APPLET_SAVE_MANAGER:
            save_manager_exit();
            break;
        // Add other applet cleanups
    }

    // Free memory
    applet_free_memory(instance);

    // Remove from registry
    for (size_t i = 0; i < g_applet_system.loaded_count; i++) {
        if (g_applet_system.loaded_applets[i] == instance) {
            g_applet_system.loaded_applets[i] = NULL;
            g_applet_system.total_memory_used -= instance->memory_size;
            break;
        }
    }

    free(instance);
    return 0;
}

Result applet_suspend(CustomAppletInstance* instance) {
    if (!instance || instance->state != CUSTOM_APPLET_STATE_ACTIVE) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }
    // Save state if needed
    if (instance->info.preserve_state) {
        applet_save_state(instance);
    }

    instance->state = CUSTOM_APPLET_STATE_SUSPENDED;
    return 0;
}

Result applet_resume(CustomAppletInstance* instance) {
    if (!instance || instance->state != CUSTOM_APPLET_STATE_SUSPENDED) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }
    // Restore state if needed
    if (instance->info.preserve_state) {
        applet_restore_state(instance);
    }

    instance->state = CUSTOM_APPLET_STATE_ACTIVE;
    return 0;
}

Result applet_check_memory_available(const AppletMemoryReq* req, bool* available) {
    if (!req || !available) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    size_t total, used, free_mem;
    Result rc = applet_get_system_memory_info(&total, &used, &free_mem);
    if (R_FAILED(rc)) {
        return rc;
    }

    // Check if we have enough memory
    *available = (free_mem >= req->min_memory + SYSTEM_RESERVED_MEMORY);
    return 0;
}

const char* applet_get_error(Result rc) {
    // Add error code handling
    return "Unknown error";
}