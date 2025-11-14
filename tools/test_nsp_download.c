#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../source/net/downloader.h"

static void progress_cb(const char* status, size_t current, size_t total) {
    if (total > 0) {
        printf("%s %zu/%zu (%.2f%%)\r", status, current, total, (double)current * 100.0 / (double)total);
    } else {
        printf("%s %zu bytes\r", status, current);
    }
    fflush(stdout);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <url> [out_path]\n", argv[0]);
        return 1;
    }
    const char* url = argv[1];
    const char* out = argc >= 3 ? argv[2] : "downloaded.nsp";

    printf("Downloading %s -> %s\n", url, out);
    int rc = download_url_to_file(url, out, progress_cb);
    if (rc == 0) {
        printf("\nDownload completed OK\n");
    } else {
        printf("\nDownload failed (rc=%d)\n", rc);
    }
    return rc;
}
