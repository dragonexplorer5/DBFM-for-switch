#ifndef HELLO_SETTINGS_H
#define HELLO_SETTINGS_H

#include <stdbool.h>

typedef struct {
    char theme[64];
    char download_dir[256];
    int confirm_installs;
    char language[16];
} AppSettings;

extern AppSettings g_settings;

void save_settings(void);
void load_settings(void);
void apply_theme(const char *name);

// Theme sequence accessors (return pointer to ANSI sequence strings)
const char *settings_get_seq_normal(void);
const char *settings_get_seq_highlight(void);
const char *settings_get_language(void);

#endif // HELLO_SETTINGS_H
