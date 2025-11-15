// simple_http.h - minimal HTTP client (GET only, no TLS)
#ifndef SIMPLE_HTTP_H
#define SIMPLE_HTTP_H

#include <stddef.h>

// Perform a simple HTTP GET. The caller must free *out_buf when successful.
// Returns 0 on success, -1 on failure.
int simple_http_get(const char *url, char **out_buf, size_t *out_len);

#endif // SIMPLE_HTTP_H
