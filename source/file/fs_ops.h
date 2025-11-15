#ifndef FS_OPS_H
#define FS_OPS_H

#include <stdbool.h>
#include <stddef.h>

// Progress/cancel handle passed to file operations so the caller (task queue / UI)
// can observe progress and request cancellation.
typedef struct {
    volatile int *progress; // 0..100, may be NULL
    volatile bool *cancel;  // pointer to cancel flag, may be NULL
} FsProgressHandle;

// Synchronous copy src -> dst. Uses SD canonicalization internally and streams data.
// Returns 0 on success, negative errno-style on failure.
int fs_copy(const char *src, const char *dst, const FsProgressHandle *handle);

// Incremental copy API: allows stepping the copy over multiple frames so UI stays
// responsive and cancellation can be observed between steps.
typedef struct FsCopyCtx FsCopyCtx;

// Begin an incremental copy. Returns 0 and allocates *out_ctx on success, negative on error.
int fs_copy_begin(const char *src, const char *dst, FsCopyCtx **out_ctx, const FsProgressHandle *handle);

// Perform up to 'max_bytes' of work. Returns:
//   0  => still in progress
//   1  => completed successfully
//  <0  => error (errno-style negative)
int fs_copy_step(FsCopyCtx *ctx, size_t max_bytes);

// Abort and free context. If 'remove_partial' is true, remove partial dst file.
void fs_copy_abort(FsCopyCtx *ctx, bool remove_partial);

// Finish and free context after completion (no-op if already finished).
void fs_copy_finish(FsCopyCtx *ctx);

// Move src -> dst. Tries rename(), falls back to copy+delete. Returns 0 on success.
int fs_move(const char *src, const char *dst, const FsProgressHandle *handle);

// Delete a file or empty directory. Returns 0 on success.
int fs_delete(const char *path);

// Create a directory (mkdir -p semantics are not required; creates single dir).
int fs_mkdir(const char *path);

// Get basic properties: size (in bytes) and is_dir flag. Returns 0 on success.
int fs_get_props(const char *path, long *out_size, int *out_is_dir);

#endif // FS_OPS_H
