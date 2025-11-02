#ifndef APPLET_LOADER_H
#define APPLET_LOADER_H

#include <switch.h>

// Applet types
typedef enum {
    APPLET_BROWSER,
    APPLET_SAVE_MANAGER,
    APPLET_SYSTEM_TOOLS,
    APPLET_FILE_MANAGER,
    APPLET_GAME_MANAGER,
    APPLET_HB_STORE,
    APPLET_TEXT_EDITOR,
    APPLET_HEX_VIEWER,
    APPLET_THEME_MANAGER,
    APPLET_SECURITY
} AppletType;

// Applet state
typedef enum {
    APPLET_STATE_UNLOADED,
    APPLET_STATE_LOADING,
    APPLET_STATE_READY,
    APPLET_STATE_ACTIVE,
    APPLET_STATE_SUSPENDED,
    APPLET_STATE_ERROR
} AppletState;

// Applet memory requirements
typedef struct {
    size_t min_memory;     // Minimum required memory
    size_t preferred_memory; // Preferred memory amount
    bool requires_gpu;     // Whether GPU access is needed
    u32 stack_size;       // Required stack size
    u32 heap_size;        // Required heap size
} AppletMemoryReq;

// Applet information
typedef struct {
    AppletType type;
    char name[32];
    char description[256];
    AppletMemoryReq memory_req;
    char version[16];
    bool auto_suspend;
    bool preserve_state;
} AppletInfo;

// Applet instance
typedef struct {
    AppletInfo info;
    AppletState state;
    void* handle;
    void* memory;
    size_t memory_size;
    void* state_data;
    size_t state_size;
} AppletInstance;

// Initialize/cleanup applet system
Result applet_loader_init(void);
void applet_loader_exit(void);

// Applet management
Result applet_load(AppletType type, AppletInstance** instance);
Result applet_unload(AppletInstance* instance);
Result applet_suspend(AppletInstance* instance);
Result applet_resume(AppletInstance* instance);
Result applet_get_state(AppletInstance* instance, AppletState* state);

// Memory management
Result applet_allocate_memory(AppletInstance* instance);
void applet_free_memory(AppletInstance* instance);
Result applet_check_memory_available(const AppletMemoryReq* req, bool* available);

// State management
Result applet_save_state(AppletInstance* instance);
Result applet_restore_state(AppletInstance* instance);
Result applet_clear_state(AppletInstance* instance);

// Information queries
Result applet_get_info(AppletType type, AppletInfo* info);
bool applet_is_available(AppletType type);
bool applet_is_loaded(AppletType type);
size_t applet_get_memory_usage(AppletInstance* instance);

// System monitoring
Result applet_get_system_memory_info(size_t* total, size_t* used, size_t* available);
Result applet_get_memory_limit(size_t* limit);
Result applet_set_memory_limit(size_t limit);

// Error handling
const char* applet_get_error(Result rc);

#endif // APPLET_LOADER_H