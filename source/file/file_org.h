#ifndef FILE_ORG_H
#define FILE_ORG_H

#include <switch.h>
#include <sys/stat.h>
#include <time.h>

// Sort modes
typedef enum {
    SORT_BY_NAME_ASC,
    SORT_BY_NAME_DESC,
    SORT_BY_DATE_ASC,
    SORT_BY_DATE_DESC,
    SORT_BY_SIZE_ASC,
    SORT_BY_SIZE_DESC,
    SORT_BY_TYPE_ASC,
    SORT_BY_TYPE_DESC
} FileSortMode;

// File filter flags
typedef enum {
    FILTER_NONE = 0,
    FILTER_NSP = (1 << 0),
    FILTER_XCI = (1 << 1),
    FILTER_NSZ = (1 << 2),
    FILTER_SAVES = (1 << 3),
    FILTER_DUMPS = (1 << 4),
    FILTER_BACKUPS = (1 << 5),
    FILTER_TEMP = (1 << 6),
    FILTER_ALL = 0xFFFFFFFF
} FileFilterFlags;

// File entry structure
typedef struct {
    char name[NAME_MAX];
    char path[PATH_MAX];
    struct stat stats;
    bool is_dir;
    bool is_selected;
    char type[32];
} FileEntry;

// Directory listing structure
typedef struct {
    FileEntry* entries;
    int count;
    int capacity;
    FileSortMode sort_mode;
    FileFilterFlags filter_flags;
    char search_term[256];
    int selected_count;
} DirListing;

// Initialize/cleanup directory listing
Result dir_listing_init(DirListing* listing);
void dir_listing_free(DirListing* listing);

// Directory operations
Result dir_create_folder(const char* path);
Result dir_rename_item(const char* old_path, const char* new_path);
Result dir_delete_item(const char* path, bool recursive);

// Listing operations
Result dir_list_files(DirListing* listing, const char* path);
void dir_sort_files(DirListing* listing, FileSortMode mode);
void dir_filter_files(DirListing* listing, FileFilterFlags flags);
void dir_search_files(DirListing* listing, const char* term);

// Selection operations
void dir_select_item(DirListing* listing, int index);
void dir_select_all(DirListing* listing);
void dir_deselect_all(DirListing* listing);
Result dir_process_selected(DirListing* listing, const char* dest_path, bool move);

// Cleanup operations
Result dir_cleanup_temps(const char* path);
Result dir_cleanup_old_backups(const char* path, int keep_count);
Result dir_cleanup_installed_nsp(const char* path);

// Helper functions
const char* get_file_type(const char* name);
bool is_temp_file(const char* name);
bool is_partial_dump(const char* name, size_t size);
bool is_redundant_backup(const char* path, time_t threshold);

#endif // FILE_ORG_H