#include "sdcard.h"
#include "../logger.h"
#include <switch.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>

// Mount SD using libnx helper. Mount point name 'sdmc' maps to sdmc: paths.
Result sdcard_mount(void) {
    Result rc = fsdevMountSdmc();
    if (R_FAILED(rc)) {
        log_event(LOG_ERROR, "sdcard: fsdevMountSdmc failed: 0x%08x", rc);
        return rc;
    }
    fsdevCommitDevice("sdmc");
    log_event(LOG_INFO, "sdcard: mounted sdmc");
    return 0;
}

// Ensure basic read access and availability of logs folder
Result sdcard_check_integrity(void) {
    DIR* d = opendir("sdmc:/");
    if (!d) {
        log_event(LOG_WARN, "sdcard: opendir(sdmc:/) failed: errno=%d", errno);
        return MAKERESULT(Module_Libnx, errno);
    }
    struct dirent* e = readdir(d);
    (void)e;
    closedir(d);
    return sdcard_ensure_logs();
}

// Helper: forbidden characters in FAT filenames
static int is_forbidden_char(unsigned char ch) {
    if (ch < 0x20) return 1; // control characters
    switch (ch) {
        case '<': case '>': case ':': case '"': case '|': case '?': case '*': case '\\':
            return 1;
        default:
            return 0;
    }
}

// Canonicalize a path into sdmc:/... form. Returns 0 on success, -1 for invalid.
int sdcard_canonicalize_path(const char* in, char* out, size_t size) {
    if (!in || !out || size == 0) return -1;

    char tmp[PATH_MAX];
    // Accept either absolute style '/' (map to sdmc:/) or explicit sdmc:/ prefix
    if (in[0] == '/') {
        if (snprintf(tmp, sizeof(tmp), "sdmc:%s", in) >= (int)sizeof(tmp)) return -1;
    } else if (strncmp(in, "sdmc:/", 6) == 0) {
        strncpy(tmp, in, sizeof(tmp)-1);
        tmp[sizeof(tmp)-1] = '\0';
    } else {
        // unknown/unsupported prefix
        return -1;
    }

    // Tokenize and normalize: remove '.' segments and properly handle '..' without escaping root
    char* tokens[PATH_MAX / 2];
    int tokc = 0;
    const char* cur = tmp + strlen("sdmc:/");

    // root
    tokens[tokc++] = strdup("sdmc");

    while (*cur) {
        const char* slash = strchr(cur, '/');
        size_t len = slash ? (size_t)(slash - cur) : strlen(cur);
        if (len > 0) {
            if (len >= PATH_MAX) goto fail;
            char seg[PATH_MAX];
            memcpy(seg, cur, len);
            seg[len] = '\0';

            // validate characters
            for (size_t i = 0; i < len; ++i) {
                unsigned char ch = (unsigned char)seg[i];
                if (is_forbidden_char(ch)) goto fail;
            }

            if (strcmp(seg, ".") == 0) {
                // skip
            } else if (strcmp(seg, "..") == 0) {
                if (tokc > 1) {
                    free(tokens[--tokc]);
                }
            } else {
                if (len > 255) goto fail;
                tokens[tokc++] = strdup(seg);
                if (tokc >= (int)(sizeof(tokens)/sizeof(tokens[0]))) goto fail;
            }
        }
        if (!slash) break;
        cur = slash + 1;
    }

    // Rebuild into out buffer
    size_t pos = 0;
    int w = snprintf(out + pos, (pos < size) ? (size - pos) : 0, "sdmc:/");
    if (w < 0) goto fail;
    pos += (size_t)w;

    for (int i = 1; i < tokc; ++i) {
        int n = snprintf(out + pos, (pos < size) ? (size - pos) : 0, "%s/", tokens[i]);
        if (n < 0) goto fail;
        pos += (size_t)n;
        if (pos >= size) goto fail;
    }

    // ensure trailing slash is present for directories (consistent with previous behavior)
    if (pos == 0 || pos >= size) goto fail;

    // free tokens
    for (int i = 0; i < tokc; ++i) free(tokens[i]);
    return 0;

fail:
    for (int i = 0; i < tokc; ++i) free(tokens[i]);
    return -1;
}

Result sdcard_ensure_logs(void) {
    int r = mkdir("sdmc:/switch/filemanager/logs", 0755);
    if (r != 0 && errno != EEXIST) {
        log_event(LOG_WARN, "sdcard: mkdir logs failed errno=%d", errno);
        return MAKERESULT(Module_Libnx, errno);
    }
    return 0;
}
