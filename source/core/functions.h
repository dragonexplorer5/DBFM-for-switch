 #ifndef HELLO_FUNCTIONS_H
 #define HELLO_FUNCTIONS_H

 #include <stddef.h>

 // Helpers used across applets
 int path_is_zip(const char *path);
 int directory_is_empty(const char *path);

 #endif // HELLO_FUNCTIONS_H
