#ifndef PARENTAL_APPLET_H
#define PARENTAL_APPLET_H

#ifdef __cplusplus
extern "C" {
#endif

// Show the parental controls applet. Returns when user exits the applet.
void parental_applet_show(int view_rows, int view_cols);

#ifdef __cplusplus
}
#endif

#endif // PARENTAL_APPLET_H