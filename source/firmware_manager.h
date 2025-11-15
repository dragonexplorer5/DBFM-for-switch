#ifndef FIRMWARE_MANAGER_H
#define FIRMWARE_MANAGER_H

#include <switch.h>
#include "compat_libnx.h"

// Firmware package information
typedef struct {
    u32 version_major;      // Major version number
    u32 version_minor;      // Minor version number
    u32 version_micro;      // Micro version number
    u32 version_padded;     // Padded version number
    char version_string[32]; // Human-readable version string
    size_t package_size;    // Total size of firmware package
    bool is_exfat;          // Whether package includes exFAT support
} FirmwareInfo;

// Initialize firmware management system
Result firmware_init(void);

// Clean up firmware management system
void firmware_exit(void);

// Get current firmware version information
Result firmware_get_version(FirmwareInfo* out_info);

// Export current firmware to a package file
Result firmware_export(const char* output_path, bool include_exfat, 
                      void (*progress_callback)(size_t current, size_t total));

// Verify firmware package integrity
Result firmware_verify_package(const char* package_path, FirmwareInfo* out_info);

// Get list of installed firmware contents
Result firmware_list_contents(char*** out_content_paths, size_t* out_count);

// Free content path list from firmware_list_contents
void firmware_free_content_list(char** content_paths, size_t count);

// Extract specific firmware file
Result firmware_extract_file(const char* content_path, const char* output_path);

// Get error message for Result code
const char* firmware_get_error_msg(Result rc);

#endif // FIRMWARE_MANAGER_H