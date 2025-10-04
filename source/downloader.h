#ifndef HELLO_DOWNLOADER_H
#define HELLO_DOWNLOADER_H

#include <stddef.h>

// Fetch the given URL into a heap-allocated buffer. On success returns 0 and sets *out and *out_len
// Caller must free(*out).
// If compiled without libcurl support (no USE_LIBCURL), returns -1.
// On success returns 0 and sets *out (malloc'd) and *out_len and optionally *content_type_out (malloc'd, may be NULL).
// Caller must free(*out) and free(*content_type_out) if non-NULL.
int http_fetch_url_to_memory(const char *url, char **out, size_t *out_len, char **content_type_out);

// Request to cancel any in-progress fetch. Thread-unsafe but used from UI loop.
void http_cancel_current(void);
// Get current download progress: returns 0 on success and fills downloaded and total (bytes). total may be 0 if unknown.
int http_get_progress(long *downloaded, long *total);

#endif // HELLO_DOWNLOADER_H
