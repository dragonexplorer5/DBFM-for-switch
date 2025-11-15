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
    char** menu_items;
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

// Homescreen rendering
void render_homescreen(int top_row, int selected_row, int view_rows, int view_cols);

// Helper functions
void ui_clear_screen(void);
void ui_refresh(void);

// UI task/progress helpers (to be called by downloader/installer code)
void ui_set_task(const char *label, int progress_percent);
void ui_clear_task(void);

// Terminal sizing probe
bool ui_probe_terminal_size(int *out_rows, int *out_cols);

// Downloads queue UI
void ui_show_downloads_queue(int view_rows, int view_cols);

// Downloads queue management (called by downloader/installer code)
// label: short user-visible name (e.g. filename)
// progress: 0-100 percent; use -1 to indicate completion/failure (removes entry)
void ui_downloads_push_update(const char *label, int progress);
void ui_downloads_remove(const char *label);

// Favorites persistence
Result ui_favorites_load(void);
Result ui_favorites_save(void);
void ui_favorites_toggle(const char *label);
bool ui_favorites_contains(const char *label);
int ui_favorites_count(void);
const char *ui_favorites_get(int idx);

#endif // UI_DATA_H