#include <stddef.h>

// Menu and UI static content used by ui.c
const char *g_menu_items[] = {
    "Files",
    "Downloads",
    "Dumps",
    "Settings",
    "Themes",
    "Exit"
};
const int g_menu_count = sizeof(g_menu_items) / sizeof(g_menu_items[0]);

const char *g_settings_lines[] = {
    "Confirm installs: (use Settings menu)",
    "Theme: (use Settings menu)",
    "Save and return"
};
const int g_settings_count = sizeof(g_settings_lines) / sizeof(g_settings_lines[0]);

const char *g_theme_lines[] = {
    "default",
    "dark",
    "blue"
};
const int g_theme_count = sizeof(g_theme_lines) / sizeof(g_theme_lines[0]);

// Dumps submenu
const char *g_dumps_menu[] = {
    "Save console dump",
    "Save file to dumps",
    "Restore from dumps",
    "Back"
};
const int g_dumps_count = sizeof(g_dumps_menu) / sizeof(g_dumps_menu[0]);
