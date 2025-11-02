#include "browser.h"
#include "fs.h"
#include <string.h>

static WebCommonConfig web_config;
static bool s_initialized = false;

Result browser_init(void) {
    if (s_initialized) return 0;

    Result rc = webPageInit();
    if (R_SUCCEEDED(rc)) {
        s_initialized = true;
    }
    return rc;
}

void browser_exit(void) {
    if (s_initialized) {
        webPageExit();
        s_initialized = false;
    }
}

Result browser_open_url(const char* url) {
    if (!s_initialized || !url) {
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    }

    Result rc = webPageCreate(&web_config, url);
    if (R_FAILED(rc)) return rc;

    // Configure browser
    webConfigSetJsExtension(&web_config, true);
    webConfigSetPageCache(&web_config, true);
    webConfigSetBootLoadingIcon(&web_config, true);
    webConfigSetFooter(&web_config, true);

    // Show web page
    rc = webConfigShow(&web_config, NULL);
    return rc;
}

Result browser_save_state(void) {
    if (!s_initialized) {
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    }

    // Save browser cache and cookies
    Result rc = webConfigSaveAll(&web_config);
    return rc;
}

Result browser_restore_state(void) {
    if (!s_initialized) {
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    }

    // Restore browser cache and cookies
    Result rc = webConfigLoadAll(&web_config);
    return rc;
}

const char* browser_get_error(Result rc) {
    if (R_SUCCEEDED(rc)) return "Success";

    switch (rc) {
        case MAKERESULT(Module_Libnx, LibnxError_NotInitialized):
            return "Browser not initialized";
        case MAKERESULT(Module_Libnx, LibnxError_BadInput):
            return "Invalid URL";
        case MAKERESULT(Module_Libnx, LibnxError_NotFound):
            return "Page not found";
        case MAKERESULT(Module_Libnx, LibnxError_OutOfMemory):
            return "Browser out of memory";
        default:
            return "Unknown browser error";
    }
}