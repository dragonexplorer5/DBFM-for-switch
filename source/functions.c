#include "functions.h"
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

int path_is_zip(const char *path) {
    if (!path) return 0;
    const char *dot = strrchr(path, '.');
    if (!dot) return 0;
    // case-insensitive compare
    if (strcasecmp(dot, ".zip") == 0) return 1;
    return 0;
}

int directory_is_empty(const char *path) {
    DIR *d = opendir(path);
    if (!d) return 0; // treat not accessible as not empty
    struct dirent *ent; int found = 0;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        found = 1; break;
    }
    closedir(d);
    return !found;
}
