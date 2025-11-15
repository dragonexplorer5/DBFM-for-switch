#include "fs_ops.h"
#include "sdcard.h"
#include "../logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <dirent.h>

// Helper: update progress safely if handle & pointer present
static void update_progress(const FsProgressHandle *h, int percent) {
    if (!h || !h->progress) return;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    *(h->progress) = percent;
}

// Internal: open file and copy with buffer, supporting cancel & progress
static int copy_stream(FILE *fsrc, FILE *fdst, size_t total_size, const FsProgressHandle *h) {
    const size_t BUF_SZ = 64 * 1024; // 64KiB
    unsigned char *buf = malloc(BUF_SZ);
    if (!buf) return -ENOMEM;
    size_t read_bytes;
    size_t copied = 0;
    while ((read_bytes = fread(buf, 1, BUF_SZ, fsrc)) > 0) {
        if (h && h->cancel && *(h->cancel)) {
            free(buf);
            return -EINTR; // cancelled
        }
        size_t wrote = fwrite(buf, 1, read_bytes, fdst);
        if (wrote != read_bytes) {
            free(buf);
            return -EIO;
        }
        copied += wrote;
        if (total_size > 0) {
            int pct = (int)((copied * 100) / total_size);
            update_progress(h, pct);
        }
    }
    free(buf);
    // finalize
    fflush(fdst);
    return 0;
}

int fs_copy(const char *src, const char *dst, const FsProgressHandle *handle) {
    if (!src || !dst) return -EINVAL;
    char csrc[PATH_MAX]; char cdst[PATH_MAX];
    if (sdcard_canonicalize_path(src, csrc, sizeof(csrc)) != 0) {
        log_event(LOG_WARN, "fs_ops: copy rejected non-sd src '%s'", src);
        return -EINVAL;
    }
    if (sdcard_canonicalize_path(dst, cdst, sizeof(cdst)) != 0) {
        log_event(LOG_WARN, "fs_ops: copy rejected non-sd dst '%s'", dst);
        return -EINVAL;
    }

    struct stat st;
    long total = 0;
    if (stat(csrc, &st) == 0) total = (long)st.st_size;

    FILE *fs = fopen(csrc, "rb");
    if (!fs) return -errno;
    FILE *fd = fopen(cdst, "wb");
    if (!fd) { fclose(fs); return -errno; }

    update_progress(handle, 0);
    int rc = copy_stream(fs, fd, (size_t)total, handle);
    fclose(fs);
    if (rc != 0) {
        // remove partial file on error / cancel
        fclose(fd);
        remove(cdst);
        return rc;
    }
    fclose(fd);
    update_progress(handle, 100);
    return 0;
}

struct FsCopyCtx {
    FILE *fsrc;
    FILE *fdst;
    size_t total;
    size_t copied;
    FsProgressHandle handle;
    char dstpath[PATH_MAX];
    unsigned char *buf;
    size_t buf_size;
};

int fs_copy_begin(const char *src, const char *dst, FsCopyCtx **out_ctx, const FsProgressHandle *handle) {
    if (!src || !dst || !out_ctx) return -EINVAL;
    char csrc[PATH_MAX]; char cdst[PATH_MAX];
    if (sdcard_canonicalize_path(src, csrc, sizeof(csrc)) != 0) return -EINVAL;
    if (sdcard_canonicalize_path(dst, cdst, sizeof(cdst)) != 0) return -EINVAL;
    FILE *fs = fopen(csrc, "rb"); if (!fs) return -errno;
    FILE *fd = fopen(cdst, "wb"); if (!fd) { int e = -errno; fclose(fs); return e; }
    struct stat st; size_t total = 0; if (stat(csrc, &st) == 0) total = (size_t)st.st_size;
    FsCopyCtx *ctx = calloc(1, sizeof(FsCopyCtx)); if (!ctx) { fclose(fs); fclose(fd); return -ENOMEM; }
    ctx->fsrc = fs; ctx->fdst = fd; ctx->total = total; ctx->copied = 0; ctx->handle.progress = NULL; ctx->handle.cancel = NULL;
    if (handle) { ctx->handle = *handle; }
    strncpy(ctx->dstpath, cdst, sizeof(ctx->dstpath)-1);
    // allocate internal buffer once
    ctx->buf_size = 32 * 1024;
    ctx->buf = malloc(ctx->buf_size);
    if (!ctx->buf) { fclose(fs); fclose(fd); free(ctx); return -ENOMEM; }
    *out_ctx = ctx;
    if (ctx->handle.progress) *(ctx->handle.progress) = 0;
    return 0;
}

int fs_copy_step(FsCopyCtx *ctx, size_t max_bytes) {
    if (!ctx) return -EINVAL;
    size_t max_chunk = ctx->buf_size;
    size_t to_do = max_bytes > 0 ? (max_bytes < max_chunk ? max_bytes : max_chunk) : max_chunk;
    size_t r = fread(ctx->buf, 1, to_do, ctx->fsrc);
    if (r == 0) {
        // EOF reached -> done
        if (ctx->handle.progress) *(ctx->handle.progress) = 100;
        return 1;
    }
    size_t w = fwrite(ctx->buf, 1, r, ctx->fdst);
    if (w != r) return -EIO;
    ctx->copied += w;
    if (ctx->total > 0 && ctx->handle.progress) {
        int pct = (int)((ctx->copied * 100) / ctx->total);
        *(ctx->handle.progress) = pct;
    }
    // check cancel
    if (ctx->handle.cancel && *(ctx->handle.cancel)) return -EINTR;
    return 0; // still running
}

void fs_copy_abort(FsCopyCtx *ctx, bool remove_partial) {
    if (!ctx) return;
    if (ctx->fsrc) fclose(ctx->fsrc);
    if (ctx->fdst) fclose(ctx->fdst);
    if (remove_partial && ctx->dstpath[0]) remove(ctx->dstpath);
    if (ctx->buf) free(ctx->buf);
    free(ctx);
}

void fs_copy_finish(FsCopyCtx *ctx) {
    if (!ctx) return;
    if (ctx->fsrc) fclose(ctx->fsrc);
    if (ctx->fdst) fclose(ctx->fdst);
    if (ctx->buf) free(ctx->buf);
    free(ctx);
}

int fs_move(const char *src, const char *dst, const FsProgressHandle *handle) {
    if (!src || !dst) return -EINVAL;
    char csrc[PATH_MAX]; char cdst[PATH_MAX];
    if (sdcard_canonicalize_path(src, csrc, sizeof(csrc)) != 0) return -EINVAL;
    if (sdcard_canonicalize_path(dst, cdst, sizeof(cdst)) != 0) return -EINVAL;

    // try rename first
    if (rename(csrc, cdst) == 0) {
        if (handle && handle->progress) *(handle->progress) = 100;
        return 0;
    }
    // fallback to copy + unlink
    int rc = fs_copy(csrc, cdst, handle);
    if (rc != 0) return rc;
    if (remove(csrc) != 0) {
        log_event(LOG_WARN, "fs_ops: moved but failed to remove src '%s'", csrc);
        // not fatal for move success from user's POV, but report nonzero
        return -EIO;
    }
    return 0;
}

int fs_delete(const char *path) {
    if (!path) return -EINVAL;
    char cpath[PATH_MAX];
    if (sdcard_canonicalize_path(path, cpath, sizeof(cpath)) != 0) return -EINVAL;
    struct stat st;
    if (stat(cpath, &st) != 0) return -errno;
    if (S_ISDIR(st.st_mode)) {
        // rmdir will fail if not empty; that's acceptable here
        if (rmdir(cpath) != 0) return -errno;
        return 0;
    } else {
        if (remove(cpath) != 0) return -errno;
        return 0;
    }
}

int fs_mkdir(const char *path) {
    if (!path) return -EINVAL;
    char cpath[PATH_MAX];
    if (sdcard_canonicalize_path(path, cpath, sizeof(cpath)) != 0) return -EINVAL;
    if (mkdir(cpath, 0755) != 0) return -errno;
    return 0;
}

int fs_get_props(const char *path, long *out_size, int *out_is_dir) {
    if (!path) return -EINVAL;
    char cpath[PATH_MAX];
    if (sdcard_canonicalize_path(path, cpath, sizeof(cpath)) != 0) return -EINVAL;
    struct stat st;
    if (stat(cpath, &st) != 0) return -errno;
    if (out_size) *out_size = (long)st.st_size;
    if (out_is_dir) *out_is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
    return 0;
}
