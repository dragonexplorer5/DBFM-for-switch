#ifndef HELLO_FILE_EXPLORER_H
#define HELLO_FILE_EXPLORER_H

// Show a directory in the file explorer UI. Returns 0 on success.
int file_explorer_open(const char *start_dir, int view_rows, int view_cols);

// Prompt action for a file; left for callbacks inside module
// Uses the shared prototype from fs.h
void prompt_file_action(int view_rows, const char *fullpath, char ***lines_buf, int *total_lines, const char *cur_dir, int *selected_row, int *top_row, int view_cols);

#endif // HELLO_FILE_EXPLORER_H
