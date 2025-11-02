#ifndef UI_DATA_H
#define UI_DATA_H

#include <switch.h>

// UI Colors
#define COLOR_TEXT 0xFFFFFFFF
#define COLOR_HIGHLIGHT 0xFF00FF00
#define COLOR_ERROR 0xFFFF0000
#define COLOR_PROGRESS 0xFF00FFFF
#define COLOR_HEADER 0xFFFFAA00

// UI Elements
typedef struct {
    char title[64];
    char subtitle[128];
    char* menu_items;
    int menu_item_count;
    int selected_index;
    int scroll_offset;
    bool show_help;
} UIState;

// Menu actions
typedef enum {
    MENU_ACTION_NONE,
    MENU_ACTION_SELECT,
    MENU_ACTION_BACK,
    MENU_ACTION_REFRESH,
    MENU_ACTION_HELP
} MenuAction;

// Initialize UI state
void ui_state_init(UIState* state);

// Set menu items
void ui_state_set_menu(UIState* state, const char** items, int count);

// Handle input for menu navigation
MenuAction ui_handle_input(UIState* state);

// Render UI elements
void ui_render_header(const UIState* state);
void ui_render_menu(const UIState* state);
void ui_render_help(const UIState* state);
void ui_render_progress(const char* operation, int progress);
void ui_render_error(const char* error);

// Helper functions
void ui_clear_screen(void);
void ui_refresh(void);

#endif // UI_DATA_H