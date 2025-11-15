/* hb_store.h - clean single header */

#ifndef HB_STORE_H
#define HB_STORE_H

#include <switch.h>
#include <limits.h>
#include "crypto.h"
#include "secure_validation.h"

// Repository structure
typedef struct {
    char name[64];
    char url[512];
    char description[1024];
    bool enabled;
    time_t last_update;
    char signature_key[256];
} Repository;

// Homebrew app metadata
typedef struct {
    char name[256];
    char title[256];
    char description[1024];
    char version[32];
    char author[128];
    char license[64];
    char category[32];
    char url[512];
    char icon_url[512];
    char screenshot_urls[5][512];
    int screenshot_count;

    /* Binary info */
    char binary_url[512];
    size_t binary_size;
    char sha256[65];
    char signature[256];

    /* Installation info */
    bool installed;
    char installed_version[32];
    char install_path[PATH_MAX];
    time_t install_date;
    bool has_update;

    /* Dependencies */
    char dependencies[16][64];
    int dependency_count;
} HomebrewApp;

// Store configuration
typedef struct {
    Repository* repositories;
    size_t repo_count;
    char cache_dir[PATH_MAX];
    bool auto_check_updates;
    u32 cache_expire_hours;
    bool verify_signatures;
    ValidationFlags validation_flags;
} StoreConfig;

// Download progress callback
typedef void (*ProgressCallback)(const char* status, size_t current, size_t total);

/* Initialization / cleanup */
Result hbstore_init(void);
void hbstore_exit(void);

/* Repository management */
Result hbstore_add_repository(const char* name, const char* url, const char* sig_key);
Result hbstore_remove_repository(const char* name);
Result hbstore_enable_repository(const char* name, bool enable);
Result hbstore_update_repositories(ProgressCallback progress_cb);
Result hbstore_get_repositories(Repository** repos, size_t* count);

/* App management */
Result hbstore_list_apps(HomebrewApp** out_apps, size_t* out_count);
Result hbstore_get_app_info(const char* app_name, HomebrewApp* out_app);
Result hbstore_download_app(const char* app_name, ProgressCallback progress_cb);
Result hbstore_install_app(const char* app_name, ProgressCallback progress_cb);
Result hbstore_uninstall_app(const char* app_name);
Result hbstore_update_app(const char* app_name, ProgressCallback progress_cb);
Result hbstore_verify_app(const char* app_name);

/* Update management */
Result hbstore_check_updates(HomebrewApp** updates, size_t* count);
Result hbstore_update_all(ProgressCallback progress_cb);
bool hbstore_has_updates(void);

/* Search functionality */
Result hbstore_search(const char* query, HomebrewApp** results, size_t* count);
Result hbstore_search_by_category(const char* category, HomebrewApp** results, size_t* count);
Result hbstore_search_by_author(const char* author, HomebrewApp** results, size_t* count);

/* Cache management */
Result hbstore_refresh_cache(void);
Result hbstore_clear_cache(void);
Result hbstore_validate_cache(void);

/* Configuration */
Result hbstore_load_config(void);
Result hbstore_save_config(void);
Result hbstore_get_config(StoreConfig* out_config);
Result hbstore_set_config(const StoreConfig* config);

/* UI helpers (optional) */
void hbstore_render_app_list(int start_row, int selected_row, const HomebrewApp* apps, size_t count);
void hbstore_render_app_details(const HomebrewApp* app);
void hbstore_render_repository_list(int start_row, int selected_row, const Repository* repos, size_t count);
void hbstore_render_update_list(int start_row, int selected_row, const HomebrewApp* updates, size_t count);

#endif // HB_STORE_H
