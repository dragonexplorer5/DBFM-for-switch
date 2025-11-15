#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include "file_org.h"
#include "task_queue.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

// Compatibility helper for strcasestr which may not be available on all platforms
static char *strcasestr_compat(const char *haystack, const char *needle) {
    if (!haystack || !needle) return NULL;
    size_t needle_len = strlen(needle);
    if (needle_len == 0) return (char*)haystack;
    for (; *haystack; ++haystack) {
        if (strncasecmp(haystack, needle, needle_len) == 0) return (char*)haystack;
    }
    return NULL;
}

// Comparison functions for sorting
static int compare_name(const void* a, const void* b, bool descending) {
    const FileEntry* fa = (const FileEntry*)a;
    const FileEntry* fb = (const FileEntry*)b;
    
    // Directories always come first
    if (fa->is_dir != fb->is_dir)
        return fa->is_dir ? -1 : 1;
    
    int result = strcasecmp(fa->name, fb->name);
    return descending ? -result : result;
}

static int compare_date(const void* a, const void* b, bool descending) {
    const FileEntry* fa = (const FileEntry*)a;
    const FileEntry* fb = (const FileEntry*)b;
    
    if (fa->is_dir != fb->is_dir)
        return fa->is_dir ? -1 : 1;
    
    time_t diff = fa->stats.st_mtime - fb->stats.st_mtime;
    return descending ? -diff : diff;
}

static int compare_size(const void* a, const void* b, bool descending) {
    const FileEntry* fa = (const FileEntry*)a;
    const FileEntry* fb = (const FileEntry*)b;
    
    if (fa->is_dir != fb->is_dir)
        return fa->is_dir ? -1 : 1;
    
    off_t diff = fa->stats.st_size - fb->stats.st_size;
    return descending ? (diff > 0 ? -1 : 1) : (diff > 0 ? 1 : -1);
}

static int compare_type(const void* a, const void* b, bool descending) {
    const FileEntry* fa = (const FileEntry*)a;
    const FileEntry* fb = (const FileEntry*)b;
    
    if (fa->is_dir != fb->is_dir)
        return fa->is_dir ? -1 : 1;
    
    int result = strcasecmp(fa->type, fb->type);
    return descending ? -result : result;
}

// Sort a simple directory listing (char** array)
void sort_directory_listing(char** entries, int count, int sort_mode) {
    if (!entries || count <= 0) return;

    // Create temporary FileEntry array for proper sorting
    FileEntry* temp_entries = malloc(count * sizeof(FileEntry));
    if (!temp_entries) return;

    // Convert string entries to FileEntry structs
    for (int i = 0; i < count; i++) {
        strncpy(temp_entries[i].name, entries[i], NAME_MAX - 1);
        temp_entries[i].name[NAME_MAX - 1] = '\0';
        
        // Get full path by combining current directory
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", ".", entries[i]); // Using current dir
        
        // Get file stats
        struct stat st;
        if (stat(full_path, &st) == 0) {
            temp_entries[i].stats = st;
        }
        
        // Check if it's a directory (ends with '/')
        size_t len = strlen(entries[i]);
        temp_entries[i].is_dir = (len > 0 && entries[i][len-1] == '/');
    }

    // Sort based on mode
    switch (sort_mode) {
        case 0: // Name
            qsort_r(temp_entries, count, sizeof(FileEntry), 
                   (int (*)(const void*, const void*, void*))compare_name, (void*)false);
            break;
        case 1: // Date
            qsort_r(temp_entries, count, sizeof(FileEntry),
                   (int (*)(const void*, const void*, void*))compare_date, (void*)false);
            break;
        case 2: // Size
            qsort_r(temp_entries, count, sizeof(FileEntry),
                   (int (*)(const void*, const void*, void*))compare_size, (void*)false);
            break;
    }

    // Copy sorted names back to original array
    for (int i = 0; i < count; i++) {
        strcpy(entries[i], temp_entries[i].name);
        if (temp_entries[i].is_dir) {
            // Ensure trailing slash for directories
            size_t len = strlen(entries[i]);
            if (len > 0 && entries[i][len-1] != '/') {
                strcat(entries[i], "/");
            }
        }
    }

    free(temp_entries);
}

// Initialize directory listing
Result dir_listing_init(DirListing* listing) {
    memset(listing, 0, sizeof(DirListing));
    listing->capacity = 100; // Initial capacity
    listing->entries = malloc(listing->capacity * sizeof(FileEntry));
    
    if (!listing->entries) return -1;
    
    listing->count = 0;
    listing->sort_mode = SORT_BY_NAME_ASC;
    listing->filter_flags = FILTER_NONE;
    listing->selected_count = 0;
    
    return 0;
}

void dir_listing_free(DirListing* listing) {
    if (listing->entries) {
        free(listing->entries);
        listing->entries = NULL;
    }
    listing->count = 0;
    listing->capacity = 0;
}

// Directory operations
Result dir_create_folder(const char* path) {
    if (mkdir(path, 0777) != 0) {
        return -errno;
    }
    return 0;
}

Result dir_rename_item(const char* old_path, const char* new_path) {
    if (rename(old_path, new_path) != 0) {
        return -errno;
    }
    return 0;
}

Result dir_delete_item(const char* path, bool recursive) {
    struct stat st;
    if (stat(path, &st) != 0) return -errno;
    
    if (S_ISDIR(st.st_mode)) {
        if (recursive) {
            DIR* dir = opendir(path);
            if (!dir) return -errno;
            
            struct dirent* entry;
            char full_path[PATH_MAX];
            Result rc = 0;
            
            while ((entry = readdir(dir)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                    continue;
                
                snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
                rc = dir_delete_item(full_path, true);
                if (R_FAILED(rc)) {
                    closedir(dir);
                    return rc;
                }
            }
            
            closedir(dir);
            if (rmdir(path) != 0) return -errno;
        } else {
            if (rmdir(path) != 0) return -errno;
        }
    } else {
        if (remove(path) != 0) return -errno;
    }
    
    return 0;
}

// Listing operations
Result dir_list_files(DirListing* listing, const char* path) {
    DIR* dir = opendir(path);
    if (!dir) return -errno;
    
    listing->count = 0;
    struct dirent* entry;
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        
        // Expand buffer if needed
        if (listing->count >= listing->capacity) {
            int new_capacity = listing->capacity * 2;
            FileEntry* new_entries = realloc(listing->entries, 
                                          new_capacity * sizeof(FileEntry));
            if (!new_entries) {
                closedir(dir);
                return -ENOMEM;
            }
            listing->entries = new_entries;
            listing->capacity = new_capacity;
        }
        
        FileEntry* curr = &listing->entries[listing->count];
        strncpy(curr->name, entry->d_name, NAME_MAX - 1);
        snprintf(curr->path, PATH_MAX, "%s/%s", path, entry->d_name);
        
        if (stat(curr->path, &curr->stats) == 0) {
            curr->is_dir = S_ISDIR(curr->stats.st_mode);
            curr->is_selected = false;
            strncpy(curr->type, get_file_type(entry->d_name), 31);
            listing->count++;
        }
    }
    
    closedir(dir);
    
    // Apply current sort mode
    dir_sort_files(listing, listing->sort_mode);
    
    return 0;
}

void dir_sort_files(DirListing* listing, FileSortMode mode) {
    listing->sort_mode = mode;
    
    switch (mode) {
        case SORT_BY_NAME_ASC:
            qsort(listing->entries, listing->count, sizeof(FileEntry),
                  (int (*)(const void*, const void*))compare_name);
            break;
            
        case SORT_BY_NAME_DESC:
            qsort(listing->entries, listing->count, sizeof(FileEntry),
                  (int (*)(const void*, const void*))compare_name);
            break;
            
        case SORT_BY_DATE_ASC:
            qsort(listing->entries, listing->count, sizeof(FileEntry),
                  (int (*)(const void*, const void*))compare_date);
            break;
            
        case SORT_BY_DATE_DESC:
            qsort(listing->entries, listing->count, sizeof(FileEntry),
                  (int (*)(const void*, const void*))compare_date);
            break;
            
        case SORT_BY_SIZE_ASC:
            qsort(listing->entries, listing->count, sizeof(FileEntry),
                  (int (*)(const void*, const void*))compare_size);
            break;
            
        case SORT_BY_SIZE_DESC:
            qsort(listing->entries, listing->count, sizeof(FileEntry),
                  (int (*)(const void*, const void*))compare_size);
            break;
            
        case SORT_BY_TYPE_ASC:
            qsort(listing->entries, listing->count, sizeof(FileEntry),
                  (int (*)(const void*, const void*))compare_type);
            break;
            
        case SORT_BY_TYPE_DESC:
            qsort(listing->entries, listing->count, sizeof(FileEntry),
                  (int (*)(const void*, const void*))compare_type);
            break;
    }
}

void dir_filter_files(DirListing* listing, FileFilterFlags flags) {
    listing->filter_flags = flags;
    
    if (flags == FILTER_NONE) return;
    
    int write_idx = 0;
    for (int read_idx = 0; read_idx < listing->count; read_idx++) {
        FileEntry* entry = &listing->entries[read_idx];
        bool keep = false;
        
        // Always keep directories
        if (entry->is_dir) {
            keep = true;
        } else {
            // Check file type against filter flags
            if ((flags & FILTER_NSP) && strstr(entry->name, ".nsp"))
                keep = true;
            else if ((flags & FILTER_XCI) && strstr(entry->name, ".xci"))
                keep = true;
            else if ((flags & FILTER_NSZ) && strstr(entry->name, ".nsz"))
                keep = true;
            else if ((flags & FILTER_SAVES) && strstr(entry->path, "/saves/"))
                keep = true;
            else if ((flags & FILTER_DUMPS) && strstr(entry->path, "/dumps/"))
                keep = true;
            else if ((flags & FILTER_BACKUPS) && strstr(entry->path, "/backups/"))
                keep = true;
            else if ((flags & FILTER_TEMP) && is_temp_file(entry->name))
                keep = true;
        }
        
        if (keep && write_idx != read_idx) {
            memcpy(&listing->entries[write_idx], entry, sizeof(FileEntry));
            write_idx++;
        }
    }
    
    listing->count = write_idx;
}

void dir_search_files(DirListing* listing, const char* term) {
    if (!term || !term[0]) return;
    
    strncpy(listing->search_term, term, sizeof(listing->search_term) - 1);
    
    int write_idx = 0;
    for (int read_idx = 0; read_idx < listing->count; read_idx++) {
        FileEntry* entry = &listing->entries[read_idx];
        
        // Case insensitive search
        if (strcasestr_compat(entry->name, term)) {
            if (write_idx != read_idx) {
                memcpy(&listing->entries[write_idx], entry, sizeof(FileEntry));
            }
            write_idx++;
        }
    }
    
    listing->count = write_idx;
}

// Selection operations
void dir_select_item(DirListing* listing, int index) {
    if (index >= 0 && index < listing->count) {
        FileEntry* entry = &listing->entries[index];
        entry->is_selected = !entry->is_selected;
        listing->selected_count += entry->is_selected ? 1 : -1;
    }
}

void dir_select_all(DirListing* listing) {
    for (int i = 0; i < listing->count; i++) {
        listing->entries[i].is_selected = true;
    }
    listing->selected_count = listing->count;
}

void dir_deselect_all(DirListing* listing) {
    for (int i = 0; i < listing->count; i++) {
        listing->entries[i].is_selected = false;
    }
    listing->selected_count = 0;
}

Result dir_process_selected(DirListing* listing, const char* dest_path, bool move) {
    Result rc = 0;
    
    for (int i = 0; i < listing->count; i++) {
        FileEntry* entry = &listing->entries[i];
        if (!entry->is_selected) continue;
        
        char dst_path[PATH_MAX];
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dest_path, entry->name);
        
        TaskType task_type = move ? TASK_MOVE : TASK_COPY;
        task_queue_add(task_type, entry->path, dst_path);
    }
    
    return rc;
}

// Helper functions
const char* get_file_type(const char* name) {
    const char* ext = strrchr(name, '.');
    if (!ext) return "unknown";
    
    ext++; // Skip the dot
    
    if (strcasecmp(ext, "nsp") == 0) return "NSP";
    if (strcasecmp(ext, "xci") == 0) return "XCI";
    if (strcasecmp(ext, "nsz") == 0) return "NSZ";
    if (strcasecmp(ext, "nro") == 0) return "NRO";
    if (strcasecmp(ext, "bin") == 0) return "BIN";
    if (strcasecmp(ext, "txt") == 0) return "TXT";
    if (strcasecmp(ext, "ini") == 0) return "INI";
    if (strcasecmp(ext, "json") == 0) return "JSON";
    
    return ext;
}

bool is_temp_file(const char* name) {
    return strstr(name, ".tmp") || 
           strstr(name, ".temp") ||
           strstr(name, ".partial") ||
           name[0] == '~' ||
           name[strlen(name)-1] == '~';
}

bool is_partial_dump(const char* name, size_t size) {
    // Check if file ends in .partial or .tmp
    if (is_temp_file(name)) return true;
    
    // Most NSP/XCI are at least 1MB
    if (strstr(name, ".nsp") || strstr(name, ".xci")) {
        return size < (1024 * 1024);
    }
    
    return false;
}

bool is_redundant_backup(const char* path, time_t threshold) {
    struct stat st;
    if (stat(path, &st) != 0) return false;
    
    // Check if it's an old backup
    if (strstr(path, "/backups/") && st.st_mtime < threshold) {
        return true;
    }
    
    return false;
}