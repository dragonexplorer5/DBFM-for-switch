#ifndef NSP_MANAGER_H
#define NSP_MANAGER_H

#include <switch.h>
#include "crypto.h"
#include "secure_validation.h"

// Package formats and types
typedef enum {
    FORMAT_NSP,
    FORMAT_XCI,
    FORMAT_NSZ,
    FORMAT_XCZ,
    FORMAT_NCA,
    FORMAT_TICKET,
    FORMAT_CERT
} PackageFormat;

typedef enum {
    PACKAGE_TYPE_BASE,
    PACKAGE_TYPE_UPDATE,
    PACKAGE_TYPE_DLC,
    PACKAGE_TYPE_DELTA
} PackageType;

// Package metadata
typedef struct {
    char name[256];
    char publisher[128];
    char version[32];
    PackageFormat format;
    PackageType type;
    u64 title_id;
    u64 title_key;
    size_t file_size;
    bool has_ticket;
    bool installed;
    char installed_version[32];
    time_t modification_time;
    u32 required_system_version;
    char icon_path[PATH_MAX];
    char control_nacp[0x4000];
} PackageMetadata;

// Installation configuration
typedef struct {
    bool ignore_firmware;
    bool ignore_required_version;
    bool install_to_nand;
    bool verify_nca;
    bool keep_certificate;
    bool remove_after_install;
    bool install_record;
    ValidationFlags validation_flags;
    char custom_name[256];
} InstallConfig;

// Network configuration
typedef struct {
    u16 port;
    char hostname[256];
    bool use_ssl;
    bool allow_remote;
    char username[32];
    char password[64];
    u32 timeout_seconds;
} NetworkConfig;

// Initialize/cleanup
Result nsp_manager_init(void);
void nsp_manager_exit(void);

// Installation operations
Result nsp_install_local(const char* path, const InstallConfig* config,
                        void (*progress_cb)(const char* status, size_t current, size_t total));
Result nsp_install_network(const char* url, const InstallConfig* config,
                         void (*progress_cb)(const char* status, size_t current, size_t total));
Result nsp_install_usb(const InstallConfig* config,
                      void (*progress_cb)(const char* status, size_t current, size_t total));
Result nsp_uninstall(u64 title_id);
Result nsp_verify(const char* path, ValidationFlags flags);

// Package analysis
Result nsp_analyze(const char* path, PackageMetadata* metadata);
Result nsp_extract(const char* path, const char* out_dir);
Result nsp_list_files(const char* path, char*** files, size_t* count);
Result nsp_get_title_key(const char* path, u64* title_key);

// Dump operations
Result nsp_dump_title(u64 title_id, const char* out_path, PackageFormat format,
                     void (*progress_cb)(const char* status, size_t current, size_t total));
Result nsp_convert(const char* in_path, const char* out_path, PackageFormat format,
                  void (*progress_cb)(const char* status, size_t current, size_t total));
Result nsp_make_ticket(u64 title_id, u64 title_key, const char* out_path);

// Network operations
Result nsp_start_server(const NetworkConfig* config);
Result nsp_stop_server(void);
bool nsp_server_is_running(void);
Result nsp_get_server_info(NetworkConfig* config);

// Title management
Result nsp_list_installed_titles(PackageMetadata** titles, size_t* count);
Result nsp_get_title_info(u64 title_id, PackageMetadata* metadata);
Result nsp_check_for_updates(const PackageMetadata* title, bool* has_update);
Result nsp_list_available_updates(PackageMetadata** updates, size_t* count);

// Title key management
Result nsp_import_title_key(u64 title_id, u64 title_key);
Result nsp_remove_title_key(u64 title_id);
Result nsp_list_title_keys(u64** title_ids, u64** title_keys, size_t* count);
Result nsp_validate_title_key(u64 title_id, u64 title_key, bool* valid);

// Space management
Result nsp_get_required_space(const char* path, u64* required_bytes);
Result nsp_get_installed_size(u64 title_id, u64* installed_bytes);

// Configuration
Result nsp_load_config(InstallConfig* config);
Result nsp_save_config(const InstallConfig* config);

// UI helpers
void nsp_render_title_list(int start_row, int selected_row,
                          const PackageMetadata* titles, size_t count);
void nsp_render_title_info(const PackageMetadata* title);
void nsp_render_install_progress(const char* status, size_t current, size_t total);

// Error handling
const char* nsp_get_error(Result rc);

#endif // NSP_MANAGER_H