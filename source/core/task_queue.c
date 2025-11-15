#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "task_queue.h"
#include <stdio.h>
#include <errno.h>
#include "../file/fs_ops.h"

static Task* task_queue_head = NULL;
static Task* task_queue_current = NULL;

void task_queue_init(void) {
    task_queue_clear();
}

int task_queue_get_aggregate_progress(void) {
    if (!task_queue_head) return 100;
    unsigned long total_est = 0;
    unsigned long processed = 0;
    Task *t = task_queue_head;
    int count = 0;
    while (t) {
        unsigned long est = (t->status.progress > 0) ? (unsigned long)(t->status.progress) : 0;
        // if we had a size field, use it; otherwise treat each task equally
        if (t->status.progress >= 0) {
            total_est += 100;
            processed += t->status.progress;
        } else {
            total_est += 100; processed += t->status.progress;
        }
        t = t->next; count++;
    }
    if (total_est == 0) return 0;
    return (int)((processed * 100) / total_est);
}

void task_queue_cancel_all(void) {
    Task *t = task_queue_head;
    while (t) { t->cancel = true; t = t->next; }
}

void task_queue_cancel_pending(void) {
    Task *t = task_queue_head;
    if (t) t = t->next; // skip current
    while (t) { t->cancel = true; t = t->next; }
}

void task_queue_add(TaskType type, const char* src, const char* dst) {
    Task* new_task = (Task*)malloc(sizeof(Task));
    if (!new_task) return;

    new_task->type = type;
    strncpy(new_task->src_path, src, PATH_MAX - 1);
    strncpy(new_task->dst_path, dst ? dst : "", PATH_MAX - 1);
    new_task->status.progress = 0;
    new_task->status.has_error = false;
    new_task->status.error_msg[0] = '\0';
    new_task->cancel = false;
    new_task->next = NULL;

    if (!task_queue_head) {
        task_queue_head = new_task;
        task_queue_current = new_task;
    } else {
        Task* last = task_queue_head;
        while (last->next) last = last->next;
        last->next = new_task;
    }
}

bool task_queue_is_empty(void) {
    return task_queue_head == NULL;
}

Task* task_queue_get_current(void) {
    return task_queue_current;
}

static void task_set_error(Task* task, const char* error) {
    task->status.has_error = true;
    strncpy(task->status.error_msg, error, sizeof(task->status.error_msg) - 1);
}

static void task_execute(Task* task) {
    int rc = 0;
    
    switch (task->type) {
        case TASK_COPY: {
            // Start incremental copy if not already started
            task->status.has_error = false;
            if (!task->op_ctx) {
                task->status.progress = 0;
                FsProgressHandle *h = malloc(sizeof(FsProgressHandle));
                if (!h) { rc = -ENOMEM; break; }
                h->progress = &task->status.progress; h->cancel = &task->cancel;
                FsCopyCtx *ctx = NULL;
                rc = fs_copy_begin(task->src_path, task->dst_path, &ctx, h);
                if (rc == 0) {
                    task->op_ctx = ctx;
                    // store the FsProgressHandle pointer in op_ctx? ctx contains a copy already, so free h
                    free(h);
                    rc = 0;
                } else {
                    free(h);
                    break;
                }
            }
            // perform one step (limit bytes per frame)
            if (task->op_ctx) {
                FsCopyCtx *ctx = (FsCopyCtx*)task->op_ctx;
                rc = fs_copy_step(ctx, 64 * 1024); // step up to 64KiB per frame
                if (rc == 1) {
                    // complete
                    fs_copy_finish(ctx);
                    task->op_ctx = NULL;
                    task->status.progress = 100;
                    rc = 0;
                } else if (rc < 0) {
                    // error or cancelled
                    if (rc == -EINTR) {
                        // cancellation requested
                        fs_copy_abort((FsCopyCtx*)task->op_ctx, true);
                        task->op_ctx = NULL;
                        rc = -ECANCELED;
                    } else {
                        fs_copy_abort((FsCopyCtx*)task->op_ctx, true);
                        task->op_ctx = NULL;
                    }
                } else {
                    // still running; return without freeing task
                    return;
                }
            }
            break;
        }
        case TASK_MOVE: {
            // implement move as incremental copy + delete
            task->status.has_error = false;
            if (!task->op_ctx) {
                task->status.progress = 0;
                FsProgressHandle *h = malloc(sizeof(FsProgressHandle));
                if (!h) { rc = -ENOMEM; break; }
                h->progress = &task->status.progress; h->cancel = &task->cancel;
                FsCopyCtx *ctx = NULL;
                rc = fs_copy_begin(task->src_path, task->dst_path, &ctx, h);
                free(h);
                if (rc == 0) {
                    task->op_ctx = ctx;
                    rc = 0;
                } else break;
            }
            if (task->op_ctx) {
                FsCopyCtx *ctx = (FsCopyCtx*)task->op_ctx;
                rc = fs_copy_step(ctx, 64 * 1024);
                if (rc == 1) {
                    fs_copy_finish(ctx); task->op_ctx = NULL;
                    // remove source
                    if (remove(task->src_path) != 0) rc = -errno;
                    else rc = 0;
                } else if (rc < 0) {
                    if (rc == -EINTR) { fs_copy_abort((FsCopyCtx*)task->op_ctx, true); task->op_ctx = NULL; rc = -ECANCELED; }
                    else { fs_copy_abort((FsCopyCtx*)task->op_ctx, true); task->op_ctx = NULL; }
                } else return;
            }
            break;
        }
        case TASK_DELETE: {
            task->status.progress = 0;
            task->status.has_error = false;
            rc = fs_delete(task->src_path);
            if (rc == 0) task->status.progress = 100;
            break;
        }
            
        case TASK_BACKUP_SAVE:
            // TODO: Implement save backup
            break;
            
        case TASK_RESTORE_SAVE:
            // TODO: Implement save restore
            break;
            
        case TASK_DUMP_NSP:
            // TODO: Implement NSP dump
            break;
            
        case TASK_INSTALL_NSP:
            // TODO: Implement NSP install
            break;
            
        case TASK_DUMP_SYSTEM:
            // TODO: Implement system dump
            break;
            
        case TASK_RESTORE_SYSTEM:
            // TODO: Implement system restore
            break;
            
        case TASK_DOWNLOAD_HB:
            // TODO: Implement homebrew download
            break;
    }
    
    if (rc != 0) {
        char error[256];
        if (rc < 0) snprintf(error, sizeof(error), "Operation failed: %s", strerror(-rc));
        else snprintf(error, sizeof(error), "Operation failed with code %d", rc);
        task_set_error(task, error);
    }
}

void task_queue_process(void) {
    if (!task_queue_current) return;

    // Execute a single step for the current task; task_execute will return
    // early if the task is still running.
    task_execute(task_queue_current);

    // If the task has no op_ctx and either completed or errored, advance
    if (!task_queue_current->op_ctx) {
        Task* completed = task_queue_current;
        task_queue_current = task_queue_current->next;
        if (completed == task_queue_head) task_queue_head = task_queue_current;
        free(completed);
    }
}

void task_queue_clear(void) {
    while (task_queue_head) {
        Task* next = task_queue_head->next;
        free(task_queue_head);
        task_queue_head = next;
    }
    task_queue_current = NULL;
}

int task_get_progress(Task* task) {
    return task ? task->status.progress : 0;
}

const char* task_get_error(Task* task) {
    return task ? task->status.error_msg : "";
}

bool task_has_error(Task* task) {
    return task ? task->status.has_error : false;
}