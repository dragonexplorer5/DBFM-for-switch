#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdbool.h>

typedef enum {
    ICON_FILE = 0,
    ICON_FOLDER,
    ICON_ZIP,
    ICON_EMPTY
} IconType;

int graphics_init(void);
void graphics_shutdown(void);

/* Draw an icon at the given visible-row and column (console coordinates).
   row: 0-based row within the visible file list (top visible row = 0)
   col: character column where the icon should start (1-based for ANSI cursor)
   type: which icon to draw
*/
void graphics_draw_icon(int row, int col, IconType type);

/* Try to preload icons from romfs/images paths; returns 0 on success or -1 if no runtime loader available.
   This is a best-effort; if icon loading not supported the ASCII fallback is used.
*/
int graphics_load_icons(void);

#endif // GRAPHICS_H
