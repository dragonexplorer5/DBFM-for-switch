#include "applet_loader.h"
#include <malloc.h>

Result applet_allocate_memory(AppletInstance* instance) {
    if (!instance) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    const AppletMemoryReq* req = &instance->info.memory_req;
    size_t total_size = req->heap_size + req->stack_size;

    // Align to page size
    total_size = (total_size + 0xFFF) & ~0xFFF;

    // Allocate memory
    void* memory = memalign(0x1000, total_size);
    if (!memory) {
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }

    instance->memory_base = memory;
    instance->memory_size = total_size;

    return 0;
}

void applet_free_memory(AppletInstance* instance) {
    if (instance && instance->memory_base) {
        free(instance->memory_base);
        instance->memory_base = NULL;
        instance->memory_size = 0;
    }
}

Result applet_get_system_memory_info(size_t* total, size_t* used, size_t* free) {
    if (!total || !used || !free) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    // Get system memory stats
    Result rc = svcGetSystemInfo(total, 6, INVALID_HANDLE, 0);
    if (R_FAILED(rc)) return rc;

    rc = svcGetSystemInfo(used, 7, INVALID_HANDLE, 0);
    if (R_FAILED(rc)) return rc;

    *free = *total - *used;
    return 0;
}