#include "procfs.h"

#include "fs.h"
#include "kconfig.h"
#include "stdlib.h"
#include "string.h"
#include "syscall.h"
#include "path.h"
#include "task.h"
#include "file.h"
#include "module.h"
#include "kernel.h"


#define PROCFS_STACK_SIZE 1024

struct procfs_file {
    int fd;
    int tid;
    int status;
    int priority;
};


void procfs_server();
void procfs_module_init();

MODULE_DECLARE(procfs, procfs_module_init);

void procfs_module_init()
{
    int tid;
    struct task_control_block *task;

    tid = kernel_create_task(procfs_server);
    if (tid < 0)
        return;

    task = task_get(tid);
    task_set_priority(task, 2);
    task_set_stack_size(task, PROCFS_STACK_SIZE);
}

void procfs_server()
{
    DECLARE_OBJECT_POOL(struct procfs_file, files, PROCFS_FILE_LIMIT);
    extern struct task_control_block _object_pool_tasks_data[];
    struct task_control_block *tasks = _object_pool_tasks_data;
    int self;
    struct fs_request request;
    int cmd;
    int from;
    int target;
    int pos;
    int size;
    int tid;
    int status;
    const char *filename;
    void *data;
    int data_start = offsetof(struct procfs_file, tid);
    int data_len = sizeof(struct procfs_file) - data_start;
    struct procfs_file *file;
    struct object_pool_cursor cursor;

    self = gettid() + 3;

    path_register_fs(PROCFS_TYPE);

    while (1) {
        if (read(self, &request, sizeof(request)) == sizeof(request)) {
            cmd = request.cmd;
            switch (cmd) {
            case FS_CMD_OPEN:
                from = request.from;
                pos = request.pos; /* searching starting position */

                status = -1;

                /* Get tid */
                filename = request.path + pos;
                if (*filename == '-' || ('0' <= *filename && *filename <= '9'))
                    tid = atoi(request.path + pos);
                else
                    tid = TASK_LIMIT;

                if (tid < TASK_LIMIT && tasks[tid].tid == tid) {
                    /* Get filename */

                    while (*filename && *filename != '/')
                        filename++;

                    if (*filename == '/') {
                        filename++;

                        if (strcmp(filename, "stat") == 0) {
                            /* Register */
                            status = path_register(request.path);

                            if (status != -1) {
                                if (mknod(status, 0, S_IFREG) == 0) {
                                    file = object_pool_allocate(&files);
                                    if (file) {
                                        file->fd = status;
                                        file->tid = tasks[tid].tid;
                                        file->status = tasks[tid].status;
                                        file->priority = tasks[tid].priority;
                                    }
                                }
                                else {
                                    status = -1;
                                }
                            }
                        }
                    }
                }

                /* Response */
                write(from, &status, sizeof(status));
                break;
            case FS_CMD_READ:
                from = request.from;
                target = request.target;
                size = request.size;
                pos = request.pos;

                /* Find fd */
                object_pool_for_each(&files, cursor, file) {
                    if (file->fd == target) {
                        data = ((void *)file) + data_start;

                        /* Check boundary */
                        if (pos < 0) {
                            status = -1;
                        }

                        if (pos > data_len) {
                            pos = data_len;
                        }

                        if (pos + size > data_len) {
                            size = data_len - pos;
                        }
                        break;
                    }
                }
                if (status == -1 || object_pool_cursor_end(&files, cursor)) {
                    write(target, NULL, -1);
                    break;
                }

                /* Response */
                write(target, data + pos, size);
                break;

            case FS_CMD_SEEK:
                target = request.target;
                size = request.size;
                pos = request.pos;

                /* Find fd */
                object_pool_for_each(&files, cursor, file) {
                    if (file->fd == target) {
                        break;
                    }
                }
                if (object_pool_cursor_end(&files, cursor)) {
                    lseek(target, -1, SEEK_SET);
                    break;
                }

                if (pos == 0) { /* SEEK_SET */
                }
                else if (pos < 0) {   /* SEEK_END */
                    size = data_len + size;
                }
                else {   /* SEEK_CUR */
                    size = pos + size;
                }
                lseek(target, size, SEEK_SET);
                break;

            case FS_CMD_CLOSE:
                target = request.target;

                /* Find fd */
                object_pool_for_each(&files, cursor, file) {
                    if (file->fd == target) {
                        object_pool_free(&files, file);
                        break;
                    }
                }
                break;

            case FS_CMD_WRITE: /* readonly */
            default:
                write(target, NULL, -1);
            }
        }
    }
}
