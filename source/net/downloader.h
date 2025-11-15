/* downloader.h */
#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <stddef.h>

// Progress callback used by streaming download functions.
// status: a short status string (e.g., "Downloading..."), current/total are bytes (total may be 0 if unknown).
typedef void (*download_progress_cb)(const char* status, size_t current, size_t total);

// Existing helper: download whole URL into memory (legacy). Caller frees *out_buf.
// Returns 0 on success, -1 on failure.
int download_url_to_memory(const char *url, char **out_buf, size_t *out_len);

// New: stream URL directly to a file on disk (out_path). Calls progress_cb periodically.
// Returns 0 on success, non-zero on error.
int download_url_to_file(const char *url, const char *out_path, download_progress_cb progress_cb);

// Convenience: a cancel function (optional) - currently a no-op placeholder.
void downloader_cancel_current(void);

#endif // DOWNLOADER_H
