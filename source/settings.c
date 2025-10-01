#include "settings.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>

static const char *settings_path = "sdmc:/switch/hello-world/settings.cfg";

AppSettings g_settings = {0};

void save_settings(void) {
    FILE *f = fopen(settings_path, "w");
    if (!f) return;
    fprintf(f, "theme=%s\n", g_settings.theme[0] ? g_settings.theme : "default");
    fprintf(f, "download_dir=%s\n", g_settings.download_dir[0] ? g_settings.download_dir : "sdmc:/switch/.tmp");
    fprintf(f, "confirm_installs=%d\n", g_settings.confirm_installs);
    fprintf(f, "language=%s\n", g_settings.language[0] ? g_settings.language : "en");
    fclose(f);
}

void load_settings(void) {
    FILE *f = fopen(settings_path, "r");
    if (!f) {
        // defaults
        strcpy(g_settings.theme, "default");
        strcpy(g_settings.download_dir, "sdmc:/switch/.tmp");
        g_settings.confirm_installs = 1;
        strcpy(g_settings.language, "en");
        return;
    }
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '='); if (!eq) continue; *eq = '\0'; char *key = line; char *val = eq + 1;
        char *nl = strchr(val, '\n'); if (nl) *nl = '\0';
        if (strcmp(key, "theme") == 0) strncpy(g_settings.theme, val, sizeof(g_settings.theme)-1);
        else if (strcmp(key, "download_dir") == 0) strncpy(g_settings.download_dir, val, sizeof(g_settings.download_dir)-1);
        else if (strcmp(key, "confirm_installs") == 0) g_settings.confirm_installs = atoi(val);
        else if (strcmp(key, "language") == 0) strncpy(g_settings.language, val, sizeof(g_settings.language)-1);
    }
    fclose(f);
}

void apply_theme(const char *name) {
    (void)name; // for now the theme name is stored; UI uses accessors to query sequences
    // Could map named themes to different sequences here in future
}

// central ANSI sequences for themes
static char seq_normal[32] = "\x1b[0m";
static char seq_highlight[32] = "\x1b[7m";

const char *settings_get_seq_normal(void) { return seq_normal; }
const char *settings_get_seq_highlight(void) { return seq_highlight; }

const char *settings_get_language(void) { return g_settings.language; }
