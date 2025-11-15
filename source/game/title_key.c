#include "title_key.h"
#include "crypto.h"
#include "fs.h"
#include <string.h>
#include <malloc.h>

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <dirent.h>
#include <unistd.h>
#include "title_key.h"
#include "common.h"
#include "compat_libnx.h"
#include "../security/crypto.h"

#define TITLEKEY_DIR "sdmc:/switch/database/title_keys"
#define TITLEKEY_DB "sdmc:/switch/dbfm/titlekeys/keys.db"

static bool s_initialized = false;

Result titlekey_init(void) {
    if (s_initialized) return 0;

    // Create titlekey directory if it doesn't exist
    fs_create_directories(TITLEKEY_DIR);

    Result rc = crypto_init();
    if (R_SUCCEEDED(rc)) {
        s_initialized = true;
    }
    return rc;
}

void titlekey_exit(void) {
    if (s_initialized) {
        crypto_exit();
        s_initialized = false;
    }
}

Result titlekey_import(const void* ticket_data, size_t ticket_size) {
    if (!s_initialized || !ticket_data || ticket_size < 0x400) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    // Extract title ID and rights ID from ticket
    u64 title_id;
    u8 rights_id[16];
    memcpy(&title_id, (u8*)ticket_data + 0x180, sizeof(title_id));
    memcpy(rights_id, (u8*)ticket_data + 0x2A0, sizeof(rights_id));

    // Extract and decrypt title key
    u8 enc_key[16];
    memcpy(enc_key, (u8*)ticket_data + 0x180, sizeof(enc_key));
    
    u8 dec_key[16];
    Result rc = crypto_decrypt_title_key(enc_key, rights_id, dec_key);
    if (R_FAILED(rc)) return rc;

    // Save to database
    char path[FS_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%016lx.key", TITLEKEY_DIR, title_id);

    FILE* f = fopen(path, "wb");
    if (!f) return MAKERESULT(Module_Libnx, LibnxError_IoError);

    bool success = true;
    success &= fwrite(&title_id, 1, sizeof(title_id), f) == sizeof(title_id);
    success &= fwrite(dec_key, 1, sizeof(dec_key), f) == sizeof(dec_key);
    success &= fwrite(rights_id, 1, sizeof(rights_id), f) == sizeof(rights_id);

    fclose(f);
    return success ? 0 : MAKERESULT(Module_Libnx, LibnxError_IoError);
}

Result titlekey_export(u64 title_id, void* out_key) {
    if (!s_initialized || !out_key) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    char path[FS_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%016lx.key", TITLEKEY_DIR, title_id);

    FILE* f = fopen(path, "rb");
    if (!f) return MAKERESULT(Module_Libnx, LibnxError_NotFound);

    u64 stored_title_id;
    u8 key[16];
    bool success = true;
    success &= fread(&stored_title_id, 1, sizeof(stored_title_id), f) == sizeof(stored_title_id);
    success &= fread(key, 1, sizeof(key), f) == sizeof(key);

    fclose(f);

    if (!success || stored_title_id != title_id) {
        return MAKERESULT(Module_Libnx, LibnxError_IoError);
    }

    memcpy(out_key, key, 16);
    return 0;
}

Result titlekey_remove(u64 title_id) {
    if (!s_initialized) {
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    }

    char path[FS_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%016lx.key", TITLEKEY_DIR, title_id);

    if (remove(path) != 0) {
        return MAKERESULT(Module_Libnx, LibnxError_IoError);
    }

    return 0;
}

Result titlekey_list(TitleKeyInfo** out_keys, size_t* out_count) {
    if (!s_initialized || !out_keys || !out_count) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    *out_keys = NULL;
    *out_count = 0;

    DIR* dir = opendir(TITLEKEY_DIR);
    if (!dir) return MAKERESULT(Module_Libnx, LibnxError_NotFound);

    size_t capacity = 16;
    TitleKeyInfo* keys = malloc(capacity * sizeof(TitleKeyInfo));
    if (!keys) {
        closedir(dir);
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }

    struct dirent* ent;
    size_t count = 0;
    while ((ent = readdir(dir))) {
        if (strlen(ent->d_name) != 20 || // 16 chars for title ID + ".key"
            strcmp(ent->d_name + 16, ".key") != 0) {
            continue;
        }

        if (count >= capacity) {
            capacity *= 2;
            TitleKeyInfo* new_keys = realloc(keys, capacity * sizeof(TitleKeyInfo));
            if (!new_keys) {
                free(keys);
                closedir(dir);
                return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
            }
            keys = new_keys;
        }

        char path[FS_MAX_PATH];
        snprintf(path, sizeof(path), "%s/%s", TITLEKEY_DIR, ent->d_name);

        FILE* f = fopen(path, "rb");
        if (f) {
            bool success = true;
            success &= fread(&keys[count].title_id, 1, sizeof(u64), f) == sizeof(u64);
            success &= fread(keys[count].key, 1, 16, f) == 16;
            success &= fread(keys[count].rights_id, 1, 16, f) == 16;
            
            if (success) {
                keys[count].in_use = true;
                count++;
            }
            fclose(f);
        }
    }

    closedir(dir);

    if (count == 0) {
        free(keys);
        *out_keys = NULL;
        *out_count = 0;
    } else {
        if (count < capacity) {
            keys = realloc(keys, count * sizeof(TitleKeyInfo));
        }
        *out_keys = keys;
        *out_count = count;
    }

    return 0;
}

void titlekey_free_list(TitleKeyInfo* keys) {
    free(keys);
}

Result titlekey_exists(u64 title_id, bool* out_exists) {
    if (!s_initialized || !out_exists) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    char path[FS_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%016lx.key", TITLEKEY_DIR, title_id);

    FILE* f = fopen(path, "rb");
    *out_exists = (f != NULL);
    if (f) fclose(f);

    return 0;
}

Result titlekey_verify(const void* key_data, size_t key_size,
                      const u8* rights_id) {
    if (!s_initialized || !key_data || !rights_id || key_size != 16) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    // TODO: Implement key verification using crypto module
    // For now, just return success
    return 0;
}

const char* titlekey_get_error(Result rc) {
    if (R_SUCCEEDED(rc)) return "Success";

    switch (rc) {
        case MAKERESULT(Module_Libnx, LibnxError_NotInitialized):
            return "Title key system not initialized";
        case MAKERESULT(Module_Libnx, LibnxError_BadInput):
            return "Invalid input parameters";
        case MAKERESULT(Module_Libnx, LibnxError_NotFound):
            return "Title key not found";
        case MAKERESULT(Module_Libnx, LibnxError_IoError):
            return "I/O error while accessing title key";
        case MAKERESULT(Module_Libnx, LibnxError_OutOfMemory):
            return "Out of memory";
        default:
            return "Unknown error";
    }
}