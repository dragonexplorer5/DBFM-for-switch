// Tiny JSON helper: extract a string value by key from a JSON buffer
#ifndef HELLO_JSON_H
#define HELLO_JSON_H

#include <stddef.h>

// Find the string value for `key` in `buf` (buf is null-terminated). Writes up to out_len-1 bytes into out.
// Returns 1 on success, 0 if not found or parse error.
int json_get_string_value(const char *buf, const char *key, char *out, size_t out_len);

#endif // HELLO_JSON_H