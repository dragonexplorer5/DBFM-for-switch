#include "fs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Minimal compat implementations for file picker helpers.
// Return a duplicated sdmc path or default. Caller must free.

char* fs_open_file_picker(const char *title, const char *filter) {
    (void)title; (void)filter;
    const char *fallback = "sdmc:/";
    char *out = strdup(fallback);
    return out;
}

char* fs_save_file_picker(const char *title, const char *default_name) {
    (void)title;
    if (!default_name) default_name = "out.bin";
    size_t len = strlen("sdmc:/") + strlen(default_name) + 1;
    char *out = malloc(len);
    if (!out) return NULL;
    snprintf(out, len, "sdmc:/%s", default_name);
    return out;
}
