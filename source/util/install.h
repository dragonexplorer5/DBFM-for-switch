#ifndef HELLO_INSTALL_H
#define HELLO_INSTALL_H

#include <stdbool.h>

typedef struct { const char *name; const char *url; const char *desc; const char *desc_en; bool installed; } InstallItem;

extern InstallItem g_candidates[];
extern const int g_candidate_count;

void scan_installs(InstallItem *items, int count);
int staged_install(const char *name, const char *url, int progress_row, int progress_cols);
int install_local_nro(const char *src_path, int progress_row, int progress_cols);

#endif // HELLO_INSTALL_H
