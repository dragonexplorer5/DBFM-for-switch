#include <stdio.h>
#include <string.h>
#include "dialog.h"
#include "ui.h"
#include <switch.h>

static bool dialog_active = false;
static bool progress_shown = false;

DialogResult dialog_show(const char* title, const char* message, DialogType type) {
    if (dialog_active) return DIALOG_CANCEL;
    dialog_active = true;
    
    // Save current screen
    consoleClear();
    
    // Draw dialog box
    printf("\x1b[10;5H┌");
    for (int i = 0; i < 70; i++) printf("─");
    printf("┐");
    
    printf("\x1b[11;5H│ %-68s │", title);
    
    printf("\x1b[12;5H├");
    for (int i = 0; i < 70; i++) printf("─");
    printf("┤");
    
    // Print message (word wrap)
    int row = 13;
    int col = 7;
    const char* ptr = message;
    int len = 0;
    
    while (*ptr) {
        if (len >= 66 || *ptr == '\n') {
            printf("\x1b[%d;5H│ %-68s │", row, "");
            row++;
            col = 7;
            len = 0;
            if (*ptr == '\n') ptr++;
            continue;
        }
        printf("\x1b[%d;%dH%c", row, col + len, *ptr);
        ptr++;
        len++;
    }
    
    // Fill remaining space
    while (row < 16) {
        printf("\x1b[%d;5H│ %-68s │", row, "");
        row++;
    }
    
    // Draw bottom border
    printf("\x1b[%d;5H└", row);
    for (int i = 0; i < 70; i++) printf("─");
    printf("┘");
    
    // Draw buttons based on dialog type
    row++;
    switch (type) {
        case DIALOG_TYPE_INFO:
            printf("\x1b[%d;35H[OK]", row);
            break;
            
        case DIALOG_TYPE_WARNING:
        case DIALOG_TYPE_ERROR:
            printf("\x1b[%d;35H[OK]", row);
            break;
            
        case DIALOG_TYPE_CONFIRM:
            printf("\x1b[%d;30H[Yes]     [No]", row);
            break;
    }
    
    // Handle input
    DialogResult result = DIALOG_CANCEL;
    bool done = false;
    
    PadState pad; padInitializeDefault(&pad); padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    while (!done) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);

        if (type == DIALOG_TYPE_CONFIRM) {
            if (kDown & HidNpadButton_A) {
                result = DIALOG_YES;
                done = true;
            }
            if (kDown & HidNpadButton_B) {
                result = DIALOG_NO;
                done = true;
            }
        } else {
            if (kDown & (HidNpadButton_A | HidNpadButton_B)) {
                result = DIALOG_OK;
                done = true;
            }
        }
        consoleUpdate(NULL);
    }
    
    dialog_active = false;
    consoleClear();
    return result;
}

DialogResult dialog_confirm_delete(const char* path) {
    char message[512];
    snprintf(message, sizeof(message),
             "Are you sure you want to delete:\n%s\n\n"
             "This operation cannot be undone!", path);
    
    return dialog_show("Confirm Delete", message, DIALOG_TYPE_CONFIRM);
}

DialogResult dialog_confirm_delete_multiple(int count) {
    char message[512];
    snprintf(message, sizeof(message),
             "Are you sure you want to delete %d items?\n\n"
             "This operation cannot be undone!", count);
    
    return dialog_show("Confirm Delete", message, DIALOG_TYPE_CONFIRM);
}

DialogResult dialog_confirm_move(const char* src, const char* dst) {
    char message[512];
    snprintf(message, sizeof(message),
             "Move:\n%s\n\nTo:\n%s", src, dst);
    
    return dialog_show("Confirm Move", message, DIALOG_TYPE_CONFIRM);
}

DialogResult dialog_confirm_cleanup(const char* operation, size_t space_to_free) {
    char message[512];
    snprintf(message, sizeof(message),
             "The following cleanup operation will be performed:\n"
             "%s\n\n"
             "This will free approximately %.2f MB\n"
             "Continue?", operation, (float)space_to_free / (1024.0f * 1024.0f));
    
    return dialog_show("Confirm Cleanup", message, DIALOG_TYPE_CONFIRM);
}

void dialog_show_error(const char* operation, Result rc) {
    char message[512];
    snprintf(message, sizeof(message),
             "Error during %s\n"
             "Result code: 0x%08X", operation, rc);
    
    dialog_show("Error", message, DIALOG_TYPE_ERROR);
}

void dialog_show_warning(const char* message) {
    dialog_show("Warning", message, DIALOG_TYPE_WARNING);
}

void dialog_show_progress(const char* operation, int progress) {
    if (dialog_active) return;
    
    if (!progress_shown) {
        consoleClear();
        progress_shown = true;
    }
    
    printf("\x1b[15;5H%-50s", operation);
    printf("\x1b[16;5H[");
    for (int i = 0; i < 48; i++) {
        printf(i < (progress * 48 / 100) ? "=" : " ");
    }
    printf("] %3d%%", progress);
}

void dialog_hide_progress(void) {
    if (progress_shown) {
        consoleClear();
        progress_shown = false;
    }
}