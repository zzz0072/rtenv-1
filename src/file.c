#include "file.h"

#include <stddef.h>
#include "kconfig.h"
#include "rt_string.h"
#include "syscall.h"
#include "fifo.h"
#include "mqueue.h"
#include "block.h"
#include "regfile.h"
#include "path.h"

int mkfile(const char *pathname, int mode, int dev)
{
    int cmd = PATH_CMD_MKFILE;
    unsigned int replyfd = gettid() + 3;
    size_t path_len = strlen(pathname)+1;
    char buf[INT_SIZE + INT_SIZE + SIZE_T_SIZE + PATH_MAX + INT_SIZE];
    (void) mode;
    int pos = 0;
    int status = 0;

    path_write_data(buf, &cmd, INT_SIZE, pos);
    path_write_data(buf, &replyfd, INT_SIZE, pos);
    path_write_data(buf, &path_len, SIZE_T_SIZE, pos);
    path_write_data(buf, pathname, path_len, pos);
    path_write_data(buf, &dev, INT_SIZE, pos);

    write(PATHSERVER_FD, buf, pos);
    read(replyfd, &status, INT_SIZE);

    return status;
}

int open(const char *pathname, int flags)
{
    int cmd = PATH_CMD_OPEN;
    unsigned int replyfd = gettid() + 3;
    size_t path_len = strlen(pathname) + 1;
    unsigned int fd = -1;
    char buf[INT_SIZE + INT_SIZE + SIZE_T_SIZE + PATH_MAX];
    (void) flags;
    int pos = 0;

    path_write_data(buf, &cmd, INT_SIZE, pos);
    path_write_data(buf, &replyfd, INT_SIZE, pos);
    path_write_data(buf, &path_len, SIZE_T_SIZE, pos);
    path_write_data(buf, pathname, path_len, pos);

    write(PATHSERVER_FD, buf, pos);
    read(replyfd, &fd, INT_SIZE);

    return fd;
}

int stat(const char *pathname, struct stat *stat)
{
    int cmd = PATH_CMD_STAT;
    unsigned int replyfd = gettid() + 3;
    size_t path_len = strlen(pathname) + 1;
    char buf[INT_SIZE + INT_SIZE + SIZE_T_SIZE + PATH_MAX];
    int pos = 0;
    int rval = 0;

    /* Send request to path server */
    path_write_data(buf, &cmd, INT_SIZE, pos);
    path_write_data(buf, &replyfd, INT_SIZE, pos);
    path_write_data(buf, &path_len, SIZE_T_SIZE, pos);
    path_write_data(buf, pathname, path_len, pos);

    write(PATHSERVER_FD, buf, pos);

    /* Response */
    read(replyfd, stat, sizeof(struct stat));
    read(replyfd, &rval, INT_SIZE);

    return rval;
}

int file_read(struct file *file, struct file_request *request,
              struct event_monitor *monitor)
{
    struct task_control_block *task = request->task;

    if (file) {
        switch (file->ops->readable(file, request, monitor)) {
            case FILE_ACCESS_ACCEPT: {
                int size = file->ops->read(file, request, monitor);

                if (task) {
                    task->stack->r0 = size;
                    task->status = TASK_READY;
                }

                return 1;
            }
            case FILE_ACCESS_BLOCK:
                if (task && task->status == TASK_READY) {
                    task->status = TASK_WAIT_READ;
                }

                return 0;
            case FILE_ACCESS_ERROR:
            default:
                ;
        }
    }

    if (task) {
        task->stack->r0 = -1;
        task->status = TASK_READY;
    }

    return -1;
}

int file_write(struct file *file, struct file_request *request,
               struct event_monitor *monitor)
{
    struct task_control_block *task = request->task;

    if (file) {
        switch (file->ops->writable(file, request, monitor)) {
            case FILE_ACCESS_ACCEPT: {
                int size = file->ops->write(file, request, monitor);

                if (task) {
                    task->stack->r0 = size;
                    task->status = TASK_READY;
                }

                return 1;
            }
            case FILE_ACCESS_BLOCK:
                if (task && task->status == TASK_READY) {
                    request->task->status = TASK_WAIT_WRITE;
                }

                return 0;
            case FILE_ACCESS_ERROR:
            default:
                ;
        }
    }

    if (task) {
        task->stack->r0 = -1;
        task->status = TASK_READY;
    }

    return -1;
}

int
file_mknod(int fd, int driver_tid, struct file *files[], int dev,
           struct memory_pool *memory_pool, struct event_monitor *event_monitor)
{
    int result;
    switch(dev) {
    case S_IFIFO:
        result = fifo_init(fd, driver_tid, files, memory_pool, event_monitor);
        break;
    case S_IMSGQ:
        result = mq_init(fd, driver_tid, files, memory_pool, event_monitor);
        break;
    case S_IFBLK:
        result = block_init(fd, driver_tid, files, memory_pool, event_monitor);
        break;
    case S_IFREG:
        result = regfile_init(fd, driver_tid, files, memory_pool, event_monitor);
        break;
    default:
        result = -1;
    }

    if (result == 0) {
        files[fd]->fd = fd;
    }

    return result;
}

int file_lseek(struct file *file, struct file_request *request,
               struct event_monitor *monitor)
{
    struct task_control_block *task = request->task;

    if (file && file->ops->lseekable && file->ops->lseek) {
        switch (file->ops->lseekable(file, request, monitor)) {
            case FILE_ACCESS_ACCEPT: {
                int status = file->ops->lseek(file, request, monitor);

                if (task) {
                    task->stack->r0 = status;
                    task->status = TASK_READY;
                }

                return 1;
            }
            case FILE_ACCESS_BLOCK:
                if (task && task->status == TASK_READY) {
                    request->task->status = TASK_WAIT_WRITE;
                }

                return 0;
            case FILE_ACCESS_ERROR:
            default:
                ;
        }
    }

    if (task) {
        task->stack->r0 = -1;
        task->status = TASK_READY;
    }

    return -1;
}

