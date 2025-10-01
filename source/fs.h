#ifndef HELLO_FS_H
#define HELLO_FS_H

#include <stddef.h>

// List directory entries into a newly-allocated array of strings. Caller must free each string and the array.
// On success returns 0 and sets *out_lines and *out_count. Each directory entry that is a directory has a trailing '/'.
int list_directory(const char *path, char ***out_lines, int *out_count);

// Prompt action menu for a file (Install/Copy/Delete/Cancel). This function may modify the lines buffer if copying or deleting files
// For simplicity this prototype mirrors usage in main.c
void prompt_file_action(int view_rows, const char *fullpath, char ***lines_buf, int *total_lines, const char *cur_dir, int *selected_row, int *top_row, int view_cols);

// Dump console text to a timestamped file under sdmc:/switch/hello-world/dumps/
int fs_dump_console_text(const char *filename_suffix, const char *text);

// Restore console text from a dump file and print it to console
int fs_restore_console_text(const char *dump_path);

// Copy a file to the dumps directory (file dump)
int fs_dump_file(const char *src_path, const char *dst_name);

// Restore a dumped file back to target directory
int fs_restore_file(const char *dump_path, const char *dst_target);

#endif // HELLO_FS_H
