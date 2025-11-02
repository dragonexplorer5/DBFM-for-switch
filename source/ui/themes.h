#ifndef THEMES_H
#define THEMES_H

#include <switch.h>

// Color definitions
typedef struct {
    u32 primary;
    u32 secondary;
    u32 accent;
    u32 text;
    u32 text_secondary;
    u32 background;
    u32 background_secondary;
    u32 success;
    u32 warning;
    u32 error;
    u32 info;
} ThemeColors;

// UI element styles
typedef struct {
    u32 background_color;
    u32 text_color;
    u32 border_color;
    bool has_border;
    u8 border_width;
    u8 padding;
    u8 margin;
    bool rounded_corners;
    u8 corner_radius;
} ElementStyle;

// Theme configuration
typedef struct {
    char name[64];
    char author[64];
    char version[32];
    char description[256];
    ThemeColors colors;
    
    // UI element styles
    ElementStyle button;
    ElementStyle menu_item;
    ElementStyle header;
    ElementStyle dialog;
    ElementStyle input;
    ElementStyle list_item;
    ElementStyle progress_bar;
    ElementStyle tab;
    ElementStyle tooltip;
    ElementStyle scrollbar;
    
    // Font configuration
    char font_main[64];
    char font_header[64];
    char font_mono[64];
    u8 font_size_normal;
    u8 font_size_small;
    u8 font_size_large;
    
    // Icons
    bool custom_icons;
    char icon_path[PATH_MAX];
    
    // Animation
    bool enable_animations;
    u16 animation_speed;
    
    // Layout
    u8 spacing;
    u8 padding;
    bool compact_mode;
} ThemeConfig;

// Initialize/cleanup theme system
Result theme_init(void);
void theme_exit(void);

// Theme management
Result theme_load(const char* name);
Result theme_save(const char* name);
Result theme_delete(const char* name);
Result theme_export(const char* name, const char* path);
Result theme_import(const char* path);
Result theme_set_default(const char* name);
Result theme_reset_to_default(void);

// Theme listing
Result theme_list_themes(char*** names, size_t* count);
Result theme_get_current(char* name, size_t name_size);
Result theme_get_default(char* name, size_t name_size);

// Theme creation and editing
Result theme_create_new(const char* name, const ThemeConfig* config);
Result theme_edit(const char* name, const ThemeConfig* config);
Result theme_get_config(const char* name, ThemeConfig* config);

// Color management
Result theme_set_colors(const ThemeColors* colors);
Result theme_get_colors(ThemeColors* colors);
u32 theme_get_color(const char* element_name);

// Style management
Result theme_set_element_style(const char* element, const ElementStyle* style);
Result theme_get_element_style(const char* element, ElementStyle* style);

// Font management
Result theme_set_font(const char* font_path, const char* font_name);
Result theme_get_font_metrics(const char* font_name, int* width, int* height);

// Icon management
Result theme_set_icon_pack(const char* path);
Result theme_get_icon_path(const char* icon_name, char* path, size_t path_size);

// Real-time preview
Result theme_apply_preview(const ThemeConfig* config);
void theme_cancel_preview(void);

// UI integration
void theme_render_preview(const ThemeConfig* config);
void theme_render_color_picker(u32* color);
void theme_render_style_editor(ElementStyle* style);
void theme_render_theme_list(int start_row, int selected_row, 
                           const char** themes, size_t count);

// Error handling
const char* theme_get_error(Result rc);

#endif // THEMES_H