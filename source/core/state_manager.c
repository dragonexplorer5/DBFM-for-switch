#include "applet_loader.h"
#include "../security/crypto.h"
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include "../../include/compat_libnx.h"

#define STATE_DIR "sdmc:/dbfm/states/"
#define STATE_FILE_TEMPLATE STATE_DIR "%s_state.bin"
#define STATE_MAX_SIZE (16 * 1024 * 1024) // 16MB max state size

typedef struct {
    uint32_t magic;           // Magic number to identify state file
    uint32_t version;         // State format version
    CustomAppletType type;    // Type of applet
    size_t data_size;        // Size of state data
    uint64_t timestamp;       // When state was saved
    uint8_t checksum[32];    // SHA-256 of state data
} StateHeader;

static const uint32_t STATE_MAGIC = 0x44424653; // "DBFS"
static const uint32_t STATE_VERSION = 1;

Result applet_save_state(CustomAppletInstance* instance) {
    if (!instance || !instance->state_data || !instance->state_size) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    // Create state directory if it doesn't exist
    fs_create_directories(STATE_DIR);

    // Prepare state file path
    char state_path[FS_MAX_PATH];
    snprintf(state_path, sizeof(state_path), STATE_FILE_TEMPLATE, instance->info.name);

    // Create state file
    FILE* f = fopen(state_path, "wb");
    if (!f) {
        return MAKERESULT(Module_Libnx, LibnxError_NotFound);
    }

    // Prepare header
    StateHeader header = {
        .magic = STATE_MAGIC,
        .version = STATE_VERSION,
        .type = instance->info.type,
        .data_size = instance->state_size,
        .timestamp = time(NULL)
    };

    // Calculate checksum
    crypto_sha256(instance->state_data, instance->state_size, header.checksum);

    // Write header
    if (fwrite(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return MAKERESULT(Module_Libnx, LibnxError_IoError);
    }

    // Write state data
    if (fwrite(instance->state_data, 1, instance->state_size, f) != instance->state_size) {
        fclose(f);
        return MAKERESULT(Module_Libnx, LibnxError_IoError);
    }

    fclose(f);
    return 0;
}

Result applet_restore_state(CustomAppletInstance* instance) {
    if (!instance) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    // Prepare state file path
    char state_path[FS_MAX_PATH];
    snprintf(state_path, sizeof(state_path), STATE_FILE_TEMPLATE, instance->info.name);

    // Open state file
    FILE* f = fopen(state_path, "rb");
    if (!f) {
        return MAKERESULT(Module_Libnx, LibnxError_NotFound);
    }

    // Read header
    StateHeader header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return MAKERESULT(Module_Libnx, LibnxError_IoError);
    }

    // Validate header
    if (header.magic != STATE_MAGIC ||
        header.version != STATE_VERSION ||
        header.type != instance->info.type ||
        header.data_size > STATE_MAX_SIZE) {
        fclose(f);
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    // Allocate memory for state
    void* state_data = malloc(header.data_size);
    if (!state_data) {
        fclose(f);
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }

    // Read state data
    if (fread(state_data, 1, header.data_size, f) != header.data_size) {
        free(state_data);
        fclose(f);
        return MAKERESULT(Module_Libnx, LibnxError_IoError);
    }

    fclose(f);

    // Verify checksum
    uint8_t checksum[32];
    crypto_sha256(state_data, header.data_size, checksum);
    if (memcmp(checksum, header.checksum, sizeof(checksum)) != 0) {
        free(state_data);
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    // Free old state if exists
    if (instance->state_data) {
        free(instance->state_data);
    }

    // Update instance
    instance->state_data = state_data;
    instance->state_size = header.data_size;

    return 0;
}