#ifndef DIALOG_H
#define DIALOG_H

#include <switch.h>
#include <stdbool.h>

typedef enum {
    DIALOG_YES,
    DIALOG_NO,
    DIALOG_CANCEL,
    DIALOG_OK
} DialogResult;

typedef enum {
    DIALOG_TYPE_INFO,
    DIALOG_TYPE_WARNING,
    DIALOG_TYPE_ERROR,
    DIALOG_TYPE_CONFIRM
} DialogType;

// Show a dialog with custom message and buttons
DialogResult dialog_show(const char* title, const char* message, DialogType type);

// Specialized confirmation dialogs
DialogResult dialog_confirm_delete(const char* path);
DialogResult dialog_confirm_delete_multiple(int count);
DialogResult dialog_confirm_move(const char* src, const char* dst);
DialogResult dialog_confirm_cleanup(const char* operation, size_t space_to_free);

// Error dialogs
void dialog_show_error(const char* operation, Result rc);
void dialog_show_warning(const char* message);

// Progress dialog
void dialog_show_progress(const char* operation, int progress);
void dialog_hide_progress(void);

#endif // DIALOG_H