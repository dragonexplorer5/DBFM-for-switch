#ifndef HELLO_FILE_EXPLORER_H
#define HELLO_FILE_EXPLORER_H

#include <switch.h>
#include "crypto.h"
#include "secure_validation.h"

// File information structure
typedef struct {
    char name[256];
    char path[PATH_MAX];
    u64 size;
    time_t modified_time;
    bool is_directory;
    bool is_hidden;
    bool is_readonly;
    char mime_type[64];
    char owner[32];
    u32 permissions;
} FileInfo;

// Search criteria
typedef struct {
    char name_pattern[256];
    char content_pattern[256];
    bool case_sensitive;
    bool regex_search;
    bool include_hidden;
    size_t min_size;
    size_t max_size;
    time_t modified_after;
    time_t modified_before;
    char file_types[16][32];
    int file_type_count;
} SearchCriteria;

// File operations configuration
typedef struct {
    bool confirm_delete;
    bool confirm_overwrite;
    bool preserve_timestamps;
    bool follow_symlinks;
    bool secure_delete;
    ValidationFlags validation_flags;
    char default_editor[PATH_MAX];
    char temp_dir[PATH_MAX];
    size_t copy_buffer_size;
} FileOpsConfig;

// Sort options
typedef enum {
    SORT_BY_NAME,
    SORT_BY_SIZE,
    SORT_BY_DATE,
    SORT_BY_TYPE,
    SORT_BY_OWNER
} SortBy;

typedef enum {
    SORT_ASCENDING,
    SORT_DESCENDING
} SortOrder;

// Initialize/cleanup
Result file_explorer_init(void);
void file_explorer_exit(void);

// Core file explorer
Result file_explorer_open(const char* start_dir, int view_rows, int view_cols);
Result file_explorer_change_dir(const char* path);
Result file_explorer_refresh(void);
const char* file_explorer_get_current_dir(void);

// File operations
Result file_copy(const char* src, const char* dst, bool overwrite,
                void (*progress_cb)(const char* status, size_t current, size_t total));
Result file_move(const char* src, const char* dst, bool overwrite);
Result file_delete(const char* path, bool secure);
Result file_rename(const char* old_path, const char* new_path);
Result file_create_dir(const char* path);
Result file_create_file(const char* path);
Result file_set_attributes(const char* path, u32 attributes);

// Advanced operations
Result file_compress(const char* path, const char* out_path,
                    void (*progress_cb)(const char* status, size_t current, size_t total));
Result file_decompress(const char* path, const char* out_path,
                      void (*progress_cb)(const char* status, size_t current, size_t total));
Result file_calculate_hash(const char* path, u8* hash, size_t hash_size,
                         void (*progress_cb)(const char* status, size_t current, size_t total));
Result file_verify_hash(const char* path, const u8* expected_hash, size_t hash_size);

// Batch operations
Result file_batch_copy(const char** src_paths, size_t count, const char* dst_dir,
                      void (*progress_cb)(const char* status, size_t current, size_t total));
Result file_batch_move(const char** src_paths, size_t count, const char* dst_dir);
Result file_batch_delete(const char** paths, size_t count);

// Search functionality
Result file_search(const SearchCriteria* criteria, FileInfo** results, size_t* count);
Result file_quick_search(const char* pattern, FileInfo** results, size_t* count);
Result file_content_search(const char* pattern, const char* path, bool recursive,
                         char*** matching_files, size_t* count);

// File information
Result file_get_info(const char* path, FileInfo* info);
Result file_list_dir(const char* path, FileInfo** entries, size_t* count,
                    SortBy sort_by, SortOrder order);
Result file_get_mime_type(const char* path, char* mime_type, size_t mime_size);
Result file_get_disk_space(const char* path, u64* free_space, u64* total_space);

// Text editor integration
Result file_open_editor(const char* path);
Result file_create_text_file(const char* path, const char* content);
Result file_append_text(const char* path, const char* content);

// Configuration
Result file_load_config(FileOpsConfig* config);
Result file_save_config(const FileOpsConfig* config);

// File action handlers
void prompt_file_action(int view_rows, const char* fullpath,
                       char*** lines_buf, int* total_lines,
                       const char* cur_dir, int* selected_row,
                       int* top_row, int view_cols);

// UI helpers
void file_render_browser(int start_row, int selected_row,
                        const FileInfo* entries, size_t count,
                        int view_rows, int view_cols);
void file_render_info_panel(const FileInfo* info);
void file_render_disk_usage(const char* path);

// Error handling
const char* file_get_error(Result rc);

#endif // HELLO_FILE_EXPLORER_H
