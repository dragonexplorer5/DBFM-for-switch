#include "graphics.h"
#include <stdio.h>
#include <string.h>

/* Simple ASCII "sprite" fallback so the project compiles and runs without extra image libs.
   If you later want PNG sprites, we will add stb_image and real framebuffer blitting.
*/

static int g_graphics_initialized = 0;

/* Simple mapping to short ASCII icons */
static const char *icon_str(IconType t) {
    switch (t) {
        case ICON_FOLDER: return "[F]";  /* folder */
        case ICON_ZIP:    return "[Z]";  /* zip folder */
        case ICON_EMPTY:  return "[ ]";  /* empty folder */
        case ICON_FILE:
        default:          return "[.]";  /* file */
    }
}

int graphics_init(void) {
    g_graphics_initialized = 1;
    return 0;
}

int graphics_load_icons(void) {
    /* Placeholder: no runtime image loader compiled by default.
       Returning -1 indicates fallback ASCII icons remain in use.
    */
    return -1;
}

void graphics_shutdown(void) {
    g_graphics_initialized = 0;
}

/* ANSI cursor positioning print. This works in the console used by libnx.
   row and col are 1-based positions in the terminal. We accept visible-row (0-based)
   and compute an approximate terminal row offset. The exact mapping depends on render_active_view
   layout; adjust the base_row if needed to align icons with text.
*/
void graphics_draw_icon(int visible_row, int col, IconType type) {
    if (!g_graphics_initialized) graphics_init();

    /* base_row: where the file list starts on the console.
       If your render_active_view uses a different offset, update BASE_ROW accordingly.
       Using 4 as a reasonable default (header lines before list). */
    const int BASE_ROW = 4;
    int term_row = BASE_ROW + visible_row;
    int term_col = col;

    const char *s = icon_str(type);

    /* Move cursor and print icon (ANSI escape sequence). */
    printf("\x1b[%d;%dH%s", term_row, term_col, s);
    fflush(stdout);
}
