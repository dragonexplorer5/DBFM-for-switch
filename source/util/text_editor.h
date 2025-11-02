#ifndef TEXT_EDITOR_H
#define TEXT_EDITOR_H

#include <switch.h>
#include "crypto.h"

// Editor modes
typedef enum {
    EDITOR_MODE_NORMAL,
    EDITOR_MODE_INSERT,
    EDITOR_MODE_SELECT,
    EDITOR_MODE_FIND,
    EDITOR_MODE_REPLACE
} EditorMode;

// Text buffer line
typedef struct {
    char* text;
    size_t length;
    size_t capacity;
    u32 syntax_flags;
} EditorLine;

// Text buffer
typedef struct {
    EditorLine* lines;
    size_t line_count;
    size_t capacity;
    char* filename;
    bool modified;
    bool read_only;
    time_t last_save;
} TextBuffer;

// Editor state
typedef struct {
    TextBuffer buffer;
    size_t cursor_x;
    size_t cursor_y;
    size_t scroll_x;
    size_t scroll_y;
    size_t screen_rows;
    size_t screen_cols;
    EditorMode mode;
    char* clipboard;
    size_t clipboard_size;
    char* find_pattern;
    bool case_sensitive;
    bool wrap_lines;
    bool show_line_numbers;
    bool auto_indent;
    bool syntax_highlight;
} EditorState;

// Editor configuration
typedef struct {
    size_t tab_size;
    bool convert_tabs;
    bool auto_save;
    u32 auto_save_interval;
    char backup_dir[PATH_MAX];
    bool create_backup;
    bool line_wrap;
    bool syntax_highlight;
    char font[32];
    u32 font_size;
} EditorConfig;

// Initialize/cleanup
Result editor_init(void);
void editor_exit(void);

// File operations
Result editor_open_file(const char* path);
Result editor_save_file(void);
Result editor_save_as(const char* path);
Result editor_reload_file(void);
Result editor_close_file(void);

// Buffer operations
Result editor_insert_char(char c);
Result editor_delete_char(void);
Result editor_insert_line(void);
Result editor_delete_line(void);
Result editor_join_lines(void);

// Cursor movement
void editor_move_cursor(int dx, int dy);
void editor_goto_line(size_t line);
void editor_scroll_page(int direction);
void editor_scroll_to_cursor(void);

// Selection and clipboard
Result editor_select_all(void);
Result editor_copy_selection(void);
Result editor_cut_selection(void);
Result editor_paste(void);
Result editor_delete_selection(void);

// Search and replace
Result editor_find_text(const char* pattern, bool case_sensitive);
Result editor_find_next(void);
Result editor_find_prev(void);
Result editor_replace_text(const char* find, const char* replace, bool all);

// Undo/Redo
Result editor_undo(void);
Result editor_redo(void);

// Mode switching
void editor_set_mode(EditorMode mode);
EditorMode editor_get_mode(void);

// Configuration
Result editor_load_config(EditorConfig* config);
Result editor_save_config(const EditorConfig* config);
Result editor_apply_config(const EditorConfig* config);

// Buffer management
Result editor_new_buffer(void);
Result editor_switch_buffer(size_t index);
size_t editor_get_buffer_count(void);
TextBuffer* editor_get_current_buffer(void);

// State queries
bool editor_is_modified(void);
bool editor_is_readonly(void);
void editor_get_cursor_position(size_t* x, size_t* y);
void editor_get_selection_range(size_t* start, size_t* end);
size_t editor_get_line_count(void);

// Rendering
void editor_render(void);
void editor_render_status_line(void);
void editor_render_message(const char* msg);
void editor_clear_message(void);

// Syntax highlighting
void editor_set_syntax_highlighting(bool enable);
void editor_highlight_line(size_t line_number);
void editor_update_syntax(void);

// Error handling
const char* editor_get_error(Result rc);

#endif // TEXT_EDITOR_H