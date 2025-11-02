#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "task_queue.h"

static Task* task_queue_head = NULL;
static Task* task_queue_current = NULL;

void task_queue_init(void) {
    task_queue_clear();
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
    Result rc = 0;
    
    switch (task->type) {
        case TASK_COPY:
            // TODO: Implement file copy with progress
            break;
            
        case TASK_MOVE:
            // TODO: Implement file move with progress
            break;
            
        case TASK_DELETE:
            // TODO: Implement file delete with progress
            break;
            
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
    
    if (R_FAILED(rc)) {
        char error[256];
        sprintf(error, "Operation failed with error 0x%x", rc);
        task_set_error(task, error);
    }
}

void task_queue_process(void) {
    if (!task_queue_current) return;
    
    task_execute(task_queue_current);
    
    // Move to next task
    Task* completed = task_queue_current;
    task_queue_current = task_queue_current->next;
    
    // Remove completed task if it's the head
    if (completed == task_queue_head) {
        task_queue_head = task_queue_current;
    }
    
    free(completed);
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