#include "file.h"

#include <stddef.h>
#include "kconfig.h"
#include "string.h"
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
    size_t plen = strlen(pathname) + 1;
    char buf[4 + 4 + 4 + PATH_MAX + 4];
    (void) mode;
    int pos = 0;
    int status = 0;

    path_write_data(buf, &cmd, 4, pos);
    path_write_data(buf, &replyfd, 4, pos);
    path_write_data(buf, &plen, 4, pos);
    path_write_data(buf, pathname, plen, pos);
    path_write_data(buf, &dev, 4, pos);

    write(PATHSERVER_FD, buf, pos);
    read(replyfd, &status, 4);

    return status;
}

int open(const char *pathname, int flags)
{
    int cmd = PATH_CMD_OPEN;
    unsigned int replyfd = gettid() + 3;
    size_t plen = strlen(pathname) + 1;
    unsigned int fd = -1;
    char buf[4 + 4 + 4 + PATH_MAX];
    (void) flags;
    int pos = 0;

    path_write_data(buf, &cmd, 4, pos);
    path_write_data(buf, &replyfd, 4, pos);
    path_write_data(buf, &plen, 4, pos);
    path_write_data(buf, pathname, plen, pos);

    write(PATHSERVER_FD, buf, pos);
    read(replyfd, &fd, 4);

    return fd;
}

int close(int fd)
{
    int cmd = PATH_CMD_CLOSE;
    unsigned int replyfd = gettid() + 3;
    int status = -1;
    char buf[4 + 4 + 4];
    int pos = 0;

    path_write_data(buf, &cmd, 4, pos);
    path_write_data(buf, &replyfd, 4, pos);
    path_write_data(buf, &fd, 4, pos);

    write(PATHSERVER_FD, buf, pos);
    read(replyfd, &status, 4);

    return status;
}

int file_release(struct event_monitor *monitor, int event,
                 struct task_control_block *task, void *data)
{
    struct file *file = data;
    struct file_request *request = (void *)task->stack->r0;

    if (FILE_EVENT_IS_READ(event))
        return file_read(file, request, monitor);
    else
        return file_write(file, request, monitor);
}

int file_read(struct file *file, struct file_request *request,
              struct event_monitor *monitor)
{
    struct task_control_block *task = request->task;

    if (file && file->ops->read) {
        int status = file->ops->read(file, request, monitor);
        switch (status) {
        default: {
            if (task) {
                task->stack->r0 = status;
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

    if (file && file->ops->write) {
        int status = file->ops->write(file, request, monitor);
        switch (status) {
        default: {
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
    switch (dev) {
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

int file_rmnod(struct file *file, struct file_request *request,
               struct event_monitor *monitor, struct file *files[])
{
    if (file && file->ops->deinit) {
        int fd = file->fd;
        int status = file->ops->deinit(file, request, monitor);

        if (request->task) {
            request->task->stack->r0 = status;
            request->task->status = TASK_READY;
        }

        files[fd] = NULL;

        return 1;
    }

    if (request->task) {
        request->task->stack->r0 = -1;
        request->task->status = TASK_READY;
    }

    return -1;
}

int file_lseek(struct file *file, struct file_request *request,
               struct event_monitor *monitor)
{
    struct task_control_block *task = request->task;

    if (file && file->ops->lseek) {
        int status = file->ops->lseek(file, request, monitor);
        switch (status) {
        default: {
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
            ;
        }
    }

    if (task) {
        task->stack->r0 = -1;
        task->status = TASK_READY;
    }

    return -1;
}

int file_mmap(struct file *file, struct file_request *request,
              struct event_monitor *monitor)
{
    struct task_control_block *task = request->task;

    if (file && file->ops->mmap) {
        int status = file->ops->mmap(file, request, monitor);
        switch (status) {
        default: {
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
            ;
        }
    }

    if (task) {
        task->stack->r0 = -1;
        task->status = TASK_READY;
    }

    return -1;
}

