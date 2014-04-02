#ifndef FILE_H
#define FILE_H

#include <stdint.h>
#include "stddef.h"
#include "task.h"
#include "event-monitor.h"
#include "memory-pool.h"

/* file types */
#define S_IFIFO (0x01)
#define S_IMSGQ (0x02)
#define S_IFBLK (0x04)
#define S_IFREG (0x08)

/* file flags */
#define O_CREAT 4

/* seek whence */
#define SEEK_SET 1
#define SEEK_CUR 2
#define SEEK_END 3

#define FILE_ACCESS_ACCEPT 1
#define FILE_ACCESS_BLOCK  0
#define FILE_ACCESS_ERROR -1

#define FILE_EVENT_READ(fd) ((fd) * 2)
#define FILE_EVENT_WRITE(fd) ((fd) * 2 + 1)

#define FILE_EVENT_IS_READ(event) ((event) % 2 == 0)

struct file_request {
    struct task_control_block *task;
    char *buf;
    int size;
    int whence;
};

struct file {
    int fd;
    struct file_operations *ops;
};

struct stat {
    uint32_t isdir;
    uint32_t len;
    uint8_t name[PATH_MAX];
};

struct file_operations {
    int (*readable)(struct file*, struct file_request*, struct event_monitor *);
    int (*writable)(struct file*, struct file_request*, struct event_monitor *);
    int (*read)(struct file*, struct file_request*, struct event_monitor *);
    int (*write)(struct file*, struct file_request*, struct event_monitor *);
    int (*lseekable)(struct file*, struct file_request*, struct event_monitor *);
    int (*lseek)(struct file*, struct file_request*, struct event_monitor *);
};

int mkfile(const char *pathname, int mode, int dev);
int open(const char *pathname, int flags);
int stat(const char *path, struct stat *buf);

int file_read(struct file *file, struct file_request *request,
              struct event_monitor *monitor);
int file_write(struct file *file, struct file_request *request,
               struct event_monitor *monitor);
int file_mknod(int fd, int driver_pid, struct file *files[], int dev,
               struct memory_pool *memory_pool,
               struct event_monitor *event_monitor);
int file_lseek(struct file *file, struct file_request *request,
               struct event_monitor *monitor);

#endif
