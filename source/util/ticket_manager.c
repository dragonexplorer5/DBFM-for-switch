#include "ticket_manager.h"
#include "crypto.h"
#include "fs.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

static const char* TICKET_MOUNTPOINT = "sdmc:/ticket";
static Result _mount_ticket_partition(void);
static Result _unmount_ticket_partition(void);

Result ticket_init(void) {
    Result rc = _mount_ticket_partition();
    if (R_FAILED(rc)) {
        return rc;
    }
    return 0;
}

void ticket_exit(void) {
    _unmount_ticket_partition();
}

Result ticket_list(TicketInfo** out_tickets, size_t* out_count) {
    if (!out_tickets || !out_count) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    // Use POSIX directory listing to avoid relying on libnx FsDir APIs which
    // may differ between libnx versions. This enumerates files under
    // TICKET_MOUNTPOINT and reads ticket headers directly.
    DIR* d = opendir(TICKET_MOUNTPOINT);
    if (!d) {
        *out_tickets = NULL;
        *out_count = 0;
        return MAKERESULT(Module_Libnx, LibnxError_NotFound);
    }

    struct dirent* ent;
    size_t count = 0;
    TicketInfo* tickets = NULL;

    while ((ent = readdir(d)) != NULL) {
        // skip . and ..
        if (ent->d_name[0] == '.') continue;

        char path[FS_MAX_PATH];
        snprintf(path, sizeof(path), "%s/%s", TICKET_MOUNTPOINT, ent->d_name);

        FILE* f = fopen(path, "rb");
        if (!f) continue;

        // Ensure space for a new ticket entry
        TicketInfo* tmp = realloc(tickets, sizeof(TicketInfo) * (count + 1));
        if (!tmp) {
            if (tickets) free(tickets);
            fclose(f);
            closedir(d);
            return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
        }
        tickets = tmp;
        // zero-init the new slot
        memset(&tickets[count], 0, sizeof(TicketInfo));

        // Read ticket header fields
        if (fseek(f, 0x180, SEEK_SET) == 0) {
            fread(&tickets[count].title_id, sizeof(u64), 1, f);
        }
        if (fseek(f, 0x207, SEEK_SET) == 0) {
            fread(&tickets[count].key_gen, sizeof(u8), 1, f);
        }
        if (fseek(f, 0x2A0, SEEK_SET) == 0) {
            fread(tickets[count].rights_id, sizeof(u8), 16, f);
        }

        tickets[count].in_use = true;
        fclose(f);
        count++;
    }

    closedir(d);

    *out_tickets = tickets;
    *out_count = count;
    return 0;
}

Result ticket_install(const char* path) {
    if (!path) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    // Read source ticket file
    FILE* src = fopen(path, "rb");
    if (!src) {
        return MAKERESULT(Module_Libnx, LibnxError_NotFound);
    }

    // Get file size
    fseek(src, 0, SEEK_END);
    size_t size = ftell(src);
    fseek(src, 0, SEEK_SET);

    // Read ticket data
    void* data = malloc(size);
    if (!data) {
        fclose(src);
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }

    if (fread(data, 1, size, src) != size) {
        free(data);
        fclose(src);
        return MAKERESULT(Module_Libnx, LibnxError_IoError);
    }
    fclose(src);

    // Get title ID from ticket
    u64 title_id;
    memcpy(&title_id, (u8*)data + 0x180, sizeof(u64));

    // Create destination path
    char dest[FS_MAX_PATH];
    snprintf(dest, sizeof(dest), "%s/%016lx.tik", TICKET_MOUNTPOINT, title_id);

    // Write ticket to destination
    FILE* dst = fopen(dest, "wb");
    if (!dst) {
        free(data);
        return MAKERESULT(Module_Libnx, LibnxError_IoError);
    }

    bool success = fwrite(data, 1, size, dst) == size;
    fclose(dst);
    free(data);

    return success ? 0 : MAKERESULT(Module_Libnx, LibnxError_IoError);
}

Result ticket_remove(const TicketInfo* ticket) {
    if (!ticket) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    char path[FS_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%016lx.tik", TICKET_MOUNTPOINT, ticket->title_id);
    
    if (remove(path) != 0) {
        return MAKERESULT(Module_Libnx, LibnxError_IoError);
    }
    
    return 0;
}

Result ticket_dump(const TicketInfo* ticket, const char* out_path) {
    if (!ticket || !out_path) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    char src_path[FS_MAX_PATH];
    snprintf(src_path, sizeof(src_path), "%s/%016lx.tik", TICKET_MOUNTPOINT, ticket->title_id);

    // Copy ticket file to destination
    FILE* src = fopen(src_path, "rb");
    if (!src) {
        return MAKERESULT(Module_Libnx, LibnxError_NotFound);
    }

    FILE* dst = fopen(out_path, "wb");
    if (!dst) {
        fclose(src);
        return MAKERESULT(Module_Libnx, LibnxError_IoError);
    }

    u8 buffer[4096];
    size_t bytes_read;
    bool success = true;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, bytes_read, dst) != bytes_read) {
            success = false;
            break;
        }
    }

    fclose(src);
    fclose(dst);

    return success ? 0 : MAKERESULT(Module_Libnx, LibnxError_IoError);
}

static Result _mount_ticket_partition(void) {
    // Mounting system save/ticket partition is platform-specific and libnx
    // APIs vary. For the purposes of this build and file-based access via
    // fopen("sdmc:/..."), no explicit mount is required here. Return
    // success so callers can proceed. If runtime mounting is needed, this
    // function should be implemented to call the appropriate libnx APIs.
    (void)TICKET_MOUNTPOINT;
    return 0;
}

static Result _unmount_ticket_partition(void) {
    // No-op unmount for compatibility reasons; real implementation may be
    // required for full runtime behavior on some systems.
    (void)TICKET_MOUNTPOINT;
    return 0;
}

Result ticket_get_common(u64 title_id, void* out_ticket, size_t* out_size) {
    char path[FS_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%016lx.tik", TICKET_MOUNTPOINT, title_id);

    FILE* f = fopen(path, "rb");
    if (!f) {
        return MAKERESULT(Module_Libnx, LibnxError_NotFound);
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (out_ticket) {
        if (fread(out_ticket, 1, size, f) != size) {
            fclose(f);
            return MAKERESULT(Module_Libnx, LibnxError_IoError);
        }
    }

    if (out_size) {
        *out_size = size;
    }

    fclose(f);
    return 0;
}

Result ticket_has_common(u64 title_id, bool* out_has) {
    if (!out_has) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    char path[FS_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%016lx.tik", TICKET_MOUNTPOINT, title_id);

    FILE* f = fopen(path, "rb");
    *out_has = (f != NULL);
    if (f) {
        fclose(f);
    }

    return 0;
}

Result ticket_get_title_key(const TicketInfo* ticket, u8* out_key) {
    if (!ticket || !out_key) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    char path[FS_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%016lx.tik", TICKET_MOUNTPOINT, ticket->title_id);

    FILE* f = fopen(path, "rb");
    if (!f) {
        return MAKERESULT(Module_Libnx, LibnxError_NotFound);
    }

    // Read encrypted title key
    u8 enc_key[16];
    fseek(f, 0x180, SEEK_SET);
    if (fread(enc_key, 1, sizeof(enc_key), f) != sizeof(enc_key)) {
        fclose(f);
        return MAKERESULT(Module_Libnx, LibnxError_IoError);
    }
    fclose(f);

    // Decrypt title key using rights ID as IV
    return crypto_decrypt_title_key(enc_key, ticket->rights_id, out_key);
}

Result ticket_import_title_key(u64 title_id, const u8* key) {
    if (!key) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    // Create blank ticket template
    u8 ticket_template[0x400] = {0};
    
    // Set title ID
    memcpy(ticket_template + 0x180, &title_id, sizeof(title_id));
    
    // Set rights ID (derive from title ID)
    u8 rights_id[16] = {0};
    memcpy(rights_id, &title_id, sizeof(title_id));
    memcpy(ticket_template + 0x2A0, rights_id, sizeof(rights_id));

    // Encrypt title key
    u8 enc_key[16];
    Result rc = crypto_encrypt_title_key(key, rights_id, enc_key);
    if (R_FAILED(rc)) {
        return rc;
    }

    // Write encrypted key to ticket
    memcpy(ticket_template + 0x180, enc_key, sizeof(enc_key));

    // Save ticket file
    char path[FS_MAX_PATH];
    snprintf(path, sizeof(path), "%s/%016lx.tik", TICKET_MOUNTPOINT, title_id);

    FILE* f = fopen(path, "wb");
    if (!f) {
        return MAKERESULT(Module_Libnx, LibnxError_IoError);
    }

    bool success = fwrite(ticket_template, 1, sizeof(ticket_template), f) == sizeof(ticket_template);
    fclose(f);

    return success ? 0 : MAKERESULT(Module_Libnx, LibnxError_IoError);
}

Result ticket_personalize(const void* ticket_data, size_t ticket_size,
                         void* out_ticket, size_t* out_size) {
    if (!ticket_data || !out_ticket || !out_size || ticket_size < 0x400) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    // Copy input ticket
    memcpy(out_ticket, ticket_data, ticket_size);
    *out_size = ticket_size;

    // Get title ID
    u64 title_id;
    memcpy(&title_id, (u8*)ticket_data + 0x180, sizeof(title_id));

    // Read device-specific keys and personalize ticket
    // Note: This is a placeholder. Actual implementation would need to use
    // device-specific keys and proper crypto operations
    return 0;
}