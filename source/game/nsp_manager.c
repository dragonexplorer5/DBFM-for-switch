#include <string.h>
#include <malloc.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include "nsp_manager.h"
#include "common.h"
#include "compat_libnx.h"
#include "../net/downloader.h"



#define NSP_BUFFER_SIZE (4 * 1024 * 1024)
#define MAX_URL_SIZE 1024

static bool server_running = false;
static int server_socket = -1;
static u8* transfer_buffer = NULL;

static Result initialize_transfer_buffer(void) {
    if (!transfer_buffer) {
        transfer_buffer = (u8*)memalign(0x1000, NSP_BUFFER_SIZE);
        if (!transfer_buffer) return -1;
    }
    return 0;
}

Result nsp_install_local(const char* path, const InstallConfig* config, void (*progress_cb)(const char* status, size_t current, size_t total)) {
    Result rc = 0;
    
    // Initialize transfer buffer
    rc = initialize_transfer_buffer();
    if (R_FAILED(rc)) return rc;
    
    // Open Content Storage
    NcmContentStorage content_storage;
    rc = ncmOpenContentStorage(&content_storage, NcmStorageId_SdCard);
    if (R_FAILED(rc)) return rc;
    
    // Open NSP file
    FILE* nsp = fopen(path, "rb");
    if (!nsp) {
        ncmContentStorageClose(&content_storage);
        return -1;
    }
    
    // Parse NSP header and extract NCAs
    NcmContentId content_id;
    NcmPlaceHolderId placeholder_id;
    
    // Get NSP file size
    fseek(nsp, 0, SEEK_END);
    // Get file size for progress tracking
    fseek(nsp, 0, SEEK_END);
    size_t total_size = ftell(nsp);
    fseek(nsp, 0, SEEK_SET);
    
    if (progress_cb) {
        progress_cb("Reading NSP header...", 0, total_size);
    }
    
    // Read and validate PFS0 header
    char pfs0_magic[4];
    if (fread(pfs0_magic, 1, 4, nsp) != 4 || memcmp(pfs0_magic, "PFS0", 4) != 0) {
        fclose(nsp);
        ncmContentStorageClose(&content_storage);
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }
    
    // Read file entry count
    u32 file_count;
    if (fread(&file_count, sizeof(u32), 1, nsp) != 1) {
        fclose(nsp);
        ncmContentStorageClose(&content_storage);
        return -3;
    }
    
    // Skip string table size and reserved
    fseek(nsp, 8, SEEK_CUR);
    
    // Read file entries
    for (u32 i = 0; i < file_count; i++) {
        u64 offset, size;
        u32 name_offset;
        if (fread(&offset, sizeof(u64), 1, nsp) != 1 ||
            fread(&size, sizeof(u64), 1, nsp) != 1 ||
            fread(&name_offset, sizeof(u32), 1, nsp) != 1) {
            break;
        }
        
        // Skip name length for now
        fseek(nsp, 4, SEEK_CUR);
        
        // Only process NCA files
        if (strstr((char*)transfer_buffer + name_offset, ".nca") != NULL) {
            // Generate placeholder ID
            arc4random_buf(&placeholder_id, sizeof(NcmPlaceHolderId));
            
            // Create placeholder
            rc = ncmContentStorageCreatePlaceHolder(&content_storage, &content_id,
                                                  &placeholder_id, size);
            if (R_FAILED(rc)) continue;
            
            // Write NCA data
            s64 written = 0;
            size_t buf_size = size > NSP_BUFFER_SIZE ? NSP_BUFFER_SIZE : size;
            
            while (written < size) {
                size_t to_read = (size - written) > buf_size ? 
                                buf_size : (size - written);
                
                if (fread(transfer_buffer, 1, to_read, nsp) != to_read) {
                    rc = -4;
                    break;
                }
                
                rc = ncmContentStorageWritePlaceHolder(&content_storage,
                     &placeholder_id, written, transfer_buffer, to_read);
                if (R_FAILED(rc)) break;
                
                written += to_read;
            }
            
            if (R_SUCCEEDED(rc)) {
                // Register placeholder
                rc = ncmContentStorageRegister(&content_storage, &content_id,
                                             &placeholder_id);
            }
            
            // Clean up on failure
            if (R_FAILED(rc)) {
                ncmContentStorageDeletePlaceHolder(&content_storage, &placeholder_id);
            }
        }
    }
    
    fclose(nsp);
    ncmContentStorageClose(&content_storage);
    return rc;
}

Result nsp_install_network(const char* url, const InstallConfig* config, void (*progress_cb)(const char* status, size_t current, size_t total)) {
    Result rc = 0;
    
    // Initialize transfer buffer
    rc = initialize_transfer_buffer();
    if (R_FAILED(rc)) return rc;
    
    if (!url) return MAKERESULT(Module_Libnx, LibnxError_BadInput);

    // Ensure download directory exists on SD
    mkdir("sdmc:/dbfm", 0755);
    mkdir("sdmc:/dbfm/downloads", 0755);

    // Derive filename from URL
    const char *last_slash = strrchr(url, '/');
    const char *name = last_slash ? last_slash + 1 : NULL;
    char tmp_path[PATH_MAX];
    if (!name || strlen(name) == 0) snprintf(tmp_path, PATH_MAX, "sdmc:/dbfm/downloads/downloaded.nsp");
    else {
        // sanitize name (very small): avoid long names
        char fname[256];
        snprintf(fname, sizeof(fname), "%s", name);
        snprintf(tmp_path, PATH_MAX, "sdmc:/dbfm/downloads/%s", fname);
    }

    // Stream download directly to tmp_path with progress callback
    int dfile_rc = download_url_to_file(url, tmp_path, progress_cb);
    if (dfile_rc != 0) {
        // remove partial file if present
        unlink(tmp_path);
        return -1;
    }

    // Call local installer on the temporary file
    rc = nsp_install_local(tmp_path, config, progress_cb);

    // Optionally remove the temporary file on success
    if (R_SUCCEEDED(rc)) {
        unlink(tmp_path);
    }

    return rc;
}

Result nsp_verify(const char* path, ValidationFlags flags) {
    Result rc = 0;
    
    // Initialize transfer buffer
    rc = initialize_transfer_buffer();
    if (R_FAILED(rc)) return rc;
    
    // TODO: Implement NSP verification
    // 1. Check NSP header integrity
    // 2. Verify NCA signatures
    // 3. Check hashes
    
    return rc;
}

Result nsp_dump_title(u64 title_id, const char* out_path, PackageFormat format, void (*progress_cb)(const char* status, size_t current, size_t total)) {
    Result rc = 0;
    
    // Initialize transfer buffer
    rc = initialize_transfer_buffer();
    if (R_FAILED(rc)) return rc;
    
    // Open Content Meta Database
    NcmContentMetaDatabase meta_db;
    rc = ncmOpenContentMetaDatabase(&meta_db, NcmStorageId_GameCard);
    if (R_FAILED(rc)) return rc;
    
    // Get content meta key
    NcmContentMetaKey meta_key = {0};
    meta_key.id = title_id;
    meta_key.type = NcmContentMetaType_Application;
    meta_key.version = 0;
    
    // Get content meta
    NcmContentStorage content_storage;
    rc = ncmOpenContentStorage(&content_storage, NcmStorageId_GameCard);
    if (R_FAILED(rc)) {
        ncmContentMetaDatabaseClose(&meta_db);
        return rc;
    }
    
    // Create output file
    char nsp_path[PATH_MAX];
    snprintf(nsp_path, PATH_MAX, "%s/%016lx.%s", out_path, title_id,
             format == FORMAT_NSZ ? "nsz" : 
             format == FORMAT_XCI ? "xci" : "nsp");
    
    FILE* out = fopen(nsp_path, "wb");
    if (!out) {
        ncmContentStorageClose(&content_storage);
        ncmContentMetaDatabaseClose(&meta_db);
        return -1;
    }
    
    // Write PFS0 header
    const char magic[4] = "PFS0";
    u32 file_count = 0;  // Will be updated later
    u32 str_table_size = 0;  // Will be updated later
    u32 reserved = 0;
    
    fwrite(magic, 1, 4, out);
    fwrite(&file_count, sizeof(u32), 1, out);
    fwrite(&str_table_size, sizeof(u32), 1, out);
    fwrite(&reserved, sizeof(u32), 1, out);
    
    // Get content records
    LegacyNcmContentRecord content_records[256];
    s32 content_count = 0;
    
    rc = ncmContentMetaDatabaseGetContentRecords(&meta_db, &meta_key,
         content_records, sizeof(content_records), &content_count);
    
    if (R_SUCCEEDED(rc)) {
        // Write content entries
        for (s32 i = 0; i < content_count; i++) {
            // Get content info
            NcmContentInfo content_info = {0};  // Initialize to safe defaults
            rc = ncmContentStorageGetContentInfo(&content_storage,
                 &content_info, &content_records[i].content_id);
            if (R_FAILED(rc)) continue;
            
            if (progress_cb) {
                char status[256];
                snprintf(status, sizeof(status), "Processing content %d/%d", i+1, content_count);
                progress_cb(status, i, content_count);
            }
            
            // Write content data
            u64 offset = 0;
            u64 remaining = content_records[i].size; // Use size from record instead of info
            
            while (remaining > 0) {
                size_t read_size = remaining > NSP_BUFFER_SIZE ? 
                                 NSP_BUFFER_SIZE : remaining;
                
                rc = ncmContentStorageReadContent(&content_storage,
                     &content_records[i].content_id, offset,
                     transfer_buffer, read_size);
                
                if (R_FAILED(rc)) break;
                
                if (format == FORMAT_NSZ) {
                    // TODO: Implement compression for NSZ format
                    fwrite(transfer_buffer, 1, read_size, out);
                } else {
                    fwrite(transfer_buffer, 1, read_size, out);
                }
                
                offset += read_size;
                remaining -= read_size;
            }
            
            file_count++;
        }
    }
    
    // Update header with final counts
    fseek(out, 4, SEEK_SET);
    fwrite(&file_count, sizeof(u32), 1, out);
    fwrite(&str_table_size, sizeof(u32), 1, out);
    
    fclose(out);
    ncmContentStorageClose(&content_storage);
    ncmContentMetaDatabaseClose(&meta_db);
    return rc;
}

Result nsp_convert(const char* in_path, const char* out_path, PackageFormat format, void (*progress_cb)(const char* status, size_t current, size_t total)) {
    Result rc = 0;
    
    // Initialize transfer buffer
    rc = initialize_transfer_buffer();
    if (R_FAILED(rc)) return rc;
    
    // TODO: Implement format conversion
    // 1. Read input package
    // 2. Convert to target format
    // 3. Write output package
    
    return rc;
}

Result nsp_start_server(const NetworkConfig* config) {
    if (server_running) return 0;
    if (!config) return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) return -1;
    
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = config->allow_remote ? INADDR_ANY : htonl(INADDR_LOOPBACK);
    server_addr.sin_port = htons(config->port);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(server_socket);
        return -2;
    }
    
    if (listen(server_socket, 5) < 0) {
        close(server_socket);
        return -3;
    }
    
    server_running = true;
    return 0;
}

Result nsp_stop_server(void) {
    if (!server_running) return 0;
    
    close(server_socket);
    server_socket = -1;
    server_running = false;
    return 0;
}

bool nsp_server_is_running(void) {
    return server_running;
}