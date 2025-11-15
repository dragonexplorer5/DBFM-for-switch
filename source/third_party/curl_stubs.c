// Minimal libcurl stubs for build-time linking on platforms where libcurl
// is not available. If libcurl is present in the portlibs, these stubs
// should be removed to link against the real library.

#include <stddef.h>

int curl_global_init(long flags) {
    (void)flags;
    return 0; // success
}

void curl_global_cleanup(void) {
    // no-op
}
