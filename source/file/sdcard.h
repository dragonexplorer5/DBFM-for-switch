#ifndef DBFM_SDCARD_H
#define DBFM_SDCARD_H

#include <switch.h>

// Mounts the SD card device (fsdev). Returns 0 on success or a libnx Result error.
Result sdcard_mount(void);

// Returns 0 if SD is accessible (can list root), non-zero Result otherwise.
Result sdcard_check_integrity(void);

// Ensure provided path is canonical and within the SD mount. Input may be like "/foo" or "sdmc:/foo".
// Writes a nul-terminated canonical path into out (size bytes). Returns 0 on success, or -1 on invalid path.
int sdcard_canonicalize_path(const char* in, char* out, size_t size);

// Create the logs directory on SD if possible.
Result sdcard_ensure_logs(void);

#endif // DBFM_SDCARD_H
