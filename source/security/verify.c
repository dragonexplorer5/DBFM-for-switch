#include "verify.h"
#include "crypto.h"
#include "fs.h"
#include "../..//include/libnx_errors.h"
#include <string.h>
#include <malloc.h>
#include <stdio.h>

#define NCA_HEADER_SIZE 0xC00
#define NSP_READ_BUFFER_SIZE 0x800000 // 8MB buffer

static bool s_initialized = false;

Result verify_init(void) {
    if (s_initialized) return 0;
    
    Result rc = crypto_init();
    if (R_SUCCEEDED(rc)) {
        s_initialized = true;
    }
    return rc;
}

void verify_exit(void) {
    if (s_initialized) {
        crypto_exit();
        s_initialized = false;
    }
}

static Result verify_nca_header(const void* header, NcaVerifyResult* out_result) {
    if (!header || !out_result) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    memset(out_result, 0, sizeof(NcaVerifyResult));

    // Verify magic "NCA3"
    if (memcmp(header, "NCA3", 4) != 0) {
        return MAKERESULT(Module_Libnx, LibnxError_BadMagic);
    }

    out_result->valid_header = true;

    // Parse header fields
    const u8* h = (const u8*)header;
    memcpy(&out_result->title_id, h + 0x210, sizeof(u64));
    out_result->content_type = *(u8*)(h + 0x205);
    out_result->crypto_type = *(u8*)(h + 0x206);
    out_result->key_gen = *(u8*)(h + 0x207);

    // Check for rights ID
    if (*(u8*)(h + 0x204) & 0x1) {
        out_result->has_rights_id = true;
        memcpy(out_result->rights_id, h + 0x230, 16);
    }

    // Determine NCA type
    switch (out_result->content_type) {
        case 0: out_result->type = NcaType_Meta; break;
        case 1: out_result->type = NcaType_Program; break;
        case 2: out_result->type = NcaType_Data; break;
        case 3: out_result->type = NcaType_Control; break;
        case 4: out_result->type = NcaType_Manual; break;
        case 5: out_result->type = NcaType_PublicData; break;
        default: return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    // Verify signature (placeholder - implement actual RSA verification)
    out_result->valid_signature = true;

    // Check if ticket is needed but missing
    if (out_result->has_rights_id) {
        bool has_ticket = false;
        Result rc = crypto_has_title_key(out_result->rights_id, &has_ticket);
        if (R_SUCCEEDED(rc)) {
            out_result->is_ticket_missing = !has_ticket;
        }
    }

    return 0;
}

Result verify_nca_file(const char* path, NcaVerifyResult* out_result) {
    if (!s_initialized || !path || !out_result) {
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    }

    FILE* f = fopen(path, "rb");
    if (!f) {
        return MAKERESULT(Module_Libnx, LibnxError_NotFound);
    }

    u8 header[NCA_HEADER_SIZE];
    size_t read = fread(header, 1, sizeof(header), f);
    fclose(f);

    if (read != sizeof(header)) {
        return MAKERESULT(Module_Libnx, LibnxError_IoError);
    }

    return verify_nca_header(header, out_result);
}

Result verify_nca_memory(const void* data, size_t size, NcaVerifyResult* out_result) {
    if (!s_initialized || !data || size < NCA_HEADER_SIZE || !out_result) {
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    }

    return verify_nca_header(data, out_result);
}

Result verify_nsp_file(const char* path, NspVerifyResult* out_result) {
    if (!s_initialized || !path || !out_result) {
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    }

    memset(out_result, 0, sizeof(NspVerifyResult));

    FILE* f = fopen(path, "rb");
    if (!f) {
        return MAKERESULT(Module_Libnx, LibnxError_NotFound);
    }

    // Read PFS0 header
    u32 magic;
    if (fread(&magic, 1, 4, f) != 4 || magic != 0x30534650) { // "PFS0"
        fclose(f);
        return MAKERESULT(Module_Libnx, LibnxError_BadMagic);
    }

    u32 num_files;
    if (fread(&num_files, 1, 4, f) != 4) {
        fclose(f);
        return MAKERESULT(Module_Libnx, LibnxError_IoError);
    }

    // Skip string table size and reserved
    fseek(f, 8, SEEK_CUR);

    // Allocate space for file entries
    size_t entries_size = num_files * 0x18;
    u8* entries = malloc(entries_size);
    if (!entries) {
        fclose(f);
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }

    if (fread(entries, 1, entries_size, f) != entries_size) {
        free(entries);
        fclose(f);
        return MAKERESULT(Module_Libnx, LibnxError_IoError);
    }

    // Get string table size and read it
    u32 string_table_size;
    fseek(f, 0x10, SEEK_SET);
    fread(&string_table_size, 1, 4, f);
    
    char* string_table = malloc(string_table_size);
    if (!string_table) {
        free(entries);
        fclose(f);
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }

    fseek(f, 0x18 + entries_size, SEEK_SET);
    if (fread(string_table, 1, string_table_size, f) != string_table_size) {
        free(string_table);
        free(entries);
        fclose(f);
        return MAKERESULT(Module_Libnx, LibnxError_IoError);
    }

    out_result->valid_format = true;
    out_result->nca_results = calloc(num_files, sizeof(NcaVerifyResult));
    if (!out_result->nca_results) {
        free(string_table);
        free(entries);
        fclose(f);
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }

    // Process each file in the NSP
    for (u32 i = 0; i < num_files; i++) {
        u64 offset = *(u64*)(entries + (i * 0x18));
        u64 size = *(u64*)(entries + (i * 0x18) + 8);
        u32 name_offset = *(u32*)(entries + (i * 0x18) + 16);
        const char* name = string_table + name_offset;

        // Check file extension
        size_t name_len = strlen(name);
        if (name_len > 4) {
            if (strcmp(name + name_len - 4, ".nca") == 0) {
                // Verify NCA
                fseek(f, offset, SEEK_SET);
                u8* buffer = malloc(NCA_HEADER_SIZE);
                if (buffer) {
                    if (fread(buffer, 1, NCA_HEADER_SIZE, f) == NCA_HEADER_SIZE) {
                        Result rc = verify_nca_header(buffer, &out_result->nca_results[out_result->nca_count]);
                        if (R_SUCCEEDED(rc)) {
                            NcaVerifyResult* nca = &out_result->nca_results[out_result->nca_count];
                            
                            // Update NSP result based on NCA type
                            switch (nca->type) {
                                case NcaType_Program:
                                    out_result->has_program = true;
                                    out_result->title_id = nca->title_id;
                                    if (nca->key_gen > out_result->min_key_gen) {
                                        out_result->min_key_gen = nca->key_gen;
                                    }
                                    break;
                                case NcaType_Control:
                                    out_result->has_control = true;
                                    break;
                                case NcaType_Meta:
                                    out_result->has_meta = true;
                                    break;
                                default:
                                    break;
                            }

                            if (nca->has_rights_id) {
                                out_result->requires_ticket = true;
                            }

                            out_result->nca_count++;
                        }
                    }
                    free(buffer);
                }
            }
            else if (strcmp(name + name_len - 4, ".tik") == 0) {
                out_result->has_ticket = true;
            }
        }
    }

    // Clean up
    free(string_table);
    free(entries);
    fclose(f);

    return 0;
}

void verify_free_nsp_result(NspVerifyResult* result) {
    if (result) {
        free(result->nca_results);
        result->nca_results = NULL;
        result->nca_count = 0;
    }
}

const char* verify_get_error_message(Result rc) {
    if (R_SUCCEEDED(rc)) return "Success";

    switch (rc) {
        case MAKERESULT(Module_Libnx, LibnxError_NotInitialized):
            return "Verification system not initialized";
        case MAKERESULT(Module_Libnx, LibnxError_BadInput):
            return "Invalid input parameters";
        case MAKERESULT(Module_Libnx, LibnxError_BadMagic):
            return "Invalid file magic (not an NCA/NSP file)";
        case MAKERESULT(Module_Libnx, LibnxError_NotFound):
            return "File not found";
        case MAKERESULT(Module_Libnx, LibnxError_IoError):
            return "I/O error while reading file";
        case MAKERESULT(Module_Libnx, LibnxError_OutOfMemory):
            return "Out of memory";
        default:
            return "Unknown error";
    }
}

const char* verify_get_content_type_string(NcaType type) {
    switch (type) {
        case NcaType_Program: return "Program";
        case NcaType_Meta: return "Meta";
        case NcaType_Control: return "Control";
        case NcaType_Manual: return "Manual";
        case NcaType_Data: return "Data";
        case NcaType_PublicData: return "Public Data";
        default: return "Unknown";
    }
}