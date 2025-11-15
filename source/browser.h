#ifndef BROWSER_H
#define BROWSER_H

#include <switch.h>
#include <limits.h>
#include <switch/applets/web.h>
#include "compat_libnx.h"
#include "crypto.h"
#include "secure_validation.h"

// Browser configuration
typedef struct {
    bool enable_javascript;
    bool enable_cookies;
    bool enable_cache;
    bool private_mode;
    char user_agent[256];
    char home_page[256];
    char download_dir[PATH_MAX];
    size_t cache_size;
    u32 timeout_seconds;
    bool verify_ssl;
    bool block_popups;
    char proxy_server[256];
    u16 proxy_port;
} BrowserConfig;

// Browser state
typedef struct {
    bool is_initialized;
    bool is_running;
    char current_url[1024];
    char page_title[256];
    u32 load_progress;
    bool is_loading;
    bool can_go_back;
    bool can_go_forward;
    u64 memory_usage;
    time_t start_time;
} BrowserState;

// Download information
typedef struct {
    char url[1024];
    char filename[256];
    size_t total_size;
    size_t downloaded_size;
    time_t start_time;
    bool is_active;
    u32 speed_bps;
    u8 progress_percent;
} DownloadInfo;

// Initialize/cleanup
Result browser_init(void);
void browser_exit(void);

// Core browser functions
Result browser_open_url(const char* url);
Result browser_close(void);
Result browser_is_available(bool* available);
Result browser_get_state(BrowserState* state);

// Navigation
Result browser_navigate(const char* url);
Result browser_go_back(void);
Result browser_go_forward(void);
Result browser_refresh(void);
Result browser_stop_loading(void);
Result browser_clear_history(void);

// Page interaction
Result browser_get_current_url(char* url, size_t url_size);
Result browser_get_page_title(char* title, size_t title_size);
Result browser_execute_javascript(const char* script, char* result, size_t result_size);
Result browser_get_page_source(char* source, size_t source_size);
Result browser_save_page(const char* path);

// State management
Result browser_save_state(void);
Result browser_restore_state(void);
Result browser_clear_state(void);
Result browser_reset(void);

// Download management
Result browser_set_download_directory(const char* path);
Result browser_get_download_directory(char* path, size_t path_size);
Result browser_start_download(const char* url, const char* filename);
Result browser_pause_download(void);
Result browser_resume_download(void);
Result browser_cancel_download(void);
Result browser_get_download_info(DownloadInfo* info);

// Configuration
Result browser_load_config(BrowserConfig* config);
Result browser_save_config(const BrowserConfig* config);
Result browser_apply_config(const BrowserConfig* config);
Result browser_reset_config(void);

// Security
Result browser_verify_ssl_certificate(bool* valid);
Result browser_set_proxy(const char* server, u16 port);
Result browser_clear_data(void);

// Cookie management
Result browser_enable_cookies(bool enable);
Result browser_clear_cookies(void);
Result browser_get_cookie(const char* domain, char* cookie, size_t cookie_size);
Result browser_set_cookie(const char* domain, const char* cookie);

// Cache management
Result browser_enable_cache(bool enable);
Result browser_clear_cache(void);
Result browser_get_cache_size(size_t* size);

// Memory management
Result browser_get_memory_usage(u64* bytes);
Result browser_cleanup_memory(void);

// Error handling
const char* browser_get_error(Result rc);

// UI helpers
void browser_render_progress(void);
void browser_render_status(const BrowserState* state);
void browser_render_download_progress(const DownloadInfo* info);
void browser_show_certificate_info(void);

#endif // BROWSER_H