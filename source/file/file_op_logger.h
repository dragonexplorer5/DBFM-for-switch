#ifndef FILE_OP_LOGGER_H
#define FILE_OP_LOGGER_H

#include <stdbool.h>

// Operation types
typedef enum {
    FILE_OP_COPY,
    FILE_OP_MOVE,
    FILE_OP_DELETE,
    FILE_OP_RENAME,
    FILE_OP_CREATE,
    FILE_OP_MKDIR
} FileOpType;

// File operation logging
void log_file_op_start(FileOpType op, const char* src, const char* dst);
void log_file_op_complete(FileOpType op, const char* src, const char* dst, bool success);
void log_file_op_error(FileOpType op, const char* src, const char* dst, const char* error);

// Operation string helpers
const char* file_op_type_str(FileOpType op);
bool file_op_is_undoable(FileOpType op);

#endif // FILE_OP_LOGGER_H