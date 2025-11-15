#ifndef APPLET_LOADER_H
#define APPLET_LOADER_H

#include <switch.h>

// Custom applet types
typedef enum {
    CUSTOM_APPLET_BROWSER,
    CUSTOM_APPLET_SAVE_MANAGER,
    CUSTOM_APPLET_SYSTEM_TOOLS,
    CUSTOM_APPLET_FILE_MANAGER,
    CUSTOM_APPLET_GAME_MANAGER,
    CUSTOM_APPLET_HB_STORE,
    CUSTOM_APPLET_TEXT_EDITOR,
    CUSTOM_APPLET_HEX_VIEWER,
    CUSTOM_APPLET_THEME_MANAGER,
    CUSTOM_APPLET_SECURITY
} CustomAppletType;

// Applet state
typedef enum {
    CUSTOM_APPLET_STATE_UNLOADED,
    CUSTOM_APPLET_STATE_LOADING,
    CUSTOM_APPLET_STATE_READY,
    CUSTOM_APPLET_STATE_ACTIVE,
    CUSTOM_APPLET_STATE_SUSPENDED,
    CUSTOM_APPLET_STATE_ERROR
} CustomAppletState;

// Applet memory requirements
typedef struct {
    size_t min_memory;     // Minimum required memory
    size_t preferred_memory; // Preferred memory amount
    bool requires_gpu;     // Whether GPU access is needed
    u32 stack_size;       // Required stack size
    u32 heap_size;        // Required heap size
} AppletMemoryReq;

// Custom applet information
typedef struct {
    CustomAppletType type;
    char name[32];
    char description[256];
    AppletMemoryReq memory_req;
    char version[16];
    bool auto_suspend;
    bool preserve_state;
} CustomAppletInfo;

// Applet instance
typedef struct {
    CustomAppletInfo info;
    CustomAppletState state;
    void* handle;
    void* memory;
    size_t memory_size;
    void* state_data;
    size_t state_size;
} CustomAppletInstance;

// Initialize/cleanup applet system
Result applet_loader_init(void);
void applet_loader_exit(void);

// Applet management
Result applet_load(CustomAppletType type, CustomAppletInstance** instance);
Result applet_unload(CustomAppletInstance* instance);
Result applet_suspend(CustomAppletInstance* instance);
Result applet_resume(CustomAppletInstance* instance);
Result applet_get_state(CustomAppletInstance* instance, CustomAppletState* state);

// Memory management
Result applet_allocate_memory(CustomAppletInstance* instance);
void applet_free_memory(CustomAppletInstance* instance);
Result applet_check_memory_available(const AppletMemoryReq* req, bool* available);

// State management
Result applet_save_state(CustomAppletInstance* instance);
Result applet_restore_state(CustomAppletInstance* instance);
Result applet_clear_state(CustomAppletInstance* instance);

// Information queries
Result applet_get_info(CustomAppletType type, CustomAppletInfo* info);
bool applet_is_available(CustomAppletType type);
bool applet_is_loaded(CustomAppletType type);
size_t applet_get_memory_usage(CustomAppletInstance* instance);

// System monitoring
Result applet_get_system_memory_info(size_t* total, size_t* used, size_t* available);
Result applet_get_memory_limit(size_t* limit);
Result applet_set_memory_limit(size_t limit);

// Error handling
const char* applet_get_error(Result rc);

#endif // APPLET_LOADER_H