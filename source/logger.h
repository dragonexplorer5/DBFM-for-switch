/* logger.h - simple logging API (single clean copy) */

#ifndef LOGGER_H
#define LOGGER_H

#include <switch.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define LOG_MAX_ENTRIES 1024
#define LOG_MAX_MESSAGE 1024

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FILE_OP,   // file operations (copy/move/delete)
    LOG_SECURITY,  // security-related events
} LogLevel;

typedef struct {
    time_t timestamp;
    LogLevel level;
    char message[LOG_MAX_MESSAGE];
} LogEntry;

// Initialize logger. Returns 0 on success.
Result logger_init(void);
// Shutdown logger and flush buffers.
void logger_exit(void);
// Log a formatted event.
Result log_event(LogLevel level, const char* fmt, ...);
// Show an in-app log viewer (UI entry point).
void logger_show_viewer(void);
// Export current in-memory logs to a file path (sdmc:/switch/...) .
Result logger_export_to_file(const char* path);

#endif // LOGGER_H
