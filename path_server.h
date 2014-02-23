#ifndef PATH_SERVER_H_20140220
#define PATH_SERVER_H_20140220
#include <stddef.h> /* size_t */
#include "task.h"

#define PIPE_BUF   64 /* Size of largest atomic pipe message */
#define PATH_MAX   32 /* Longest absolute path */
#define PIPE_LIMIT (TASK_LIMIT * 2)

#define PATHSERVER_FD (TASK_LIMIT + 3)
/* File descriptor of pipe to pathserver */

#define S_IFIFO 1
#define S_IMSGQ 2

#define O_CREAT 4

#define REQ_RD (1)
#define REQ_WR (2)

struct pipe_ringbuffer {
    int start;
    int end;
    int req_st;
    char data[PIPE_BUF];

    int (*readable) (struct pipe_ringbuffer*, struct task_control_block*);
    int (*writable) (struct pipe_ringbuffer*, struct task_control_block*);
    int (*read) (struct pipe_ringbuffer*, struct task_control_block*);
    int (*write) (struct pipe_ringbuffer*, struct task_control_block*);
};

void pathserver();
void _read(struct task_control_block *task, struct task_control_block *tasks, size_t task_count, struct pipe_ringbuffer *pipes);
void _write(struct task_control_block *task, struct task_control_block *tasks, size_t task_count, struct pipe_ringbuffer *pipes);

int mkfile(const char *pathname, int mode, int dev);
int mkfifo(const char *pathname, int mode);
int open(const char *pathname, int flags);
int mq_open(const char *name, int oflag);
int fifo_readable (struct pipe_ringbuffer *pipe, struct task_control_block *task);
int mq_readable (struct pipe_ringbuffer *pipe, struct task_control_block *task);
int fifo_read (struct pipe_ringbuffer *pipe, struct task_control_block *task);
int mq_read (struct pipe_ringbuffer *pipe, struct task_control_block *task);
int fifo_writable (struct pipe_ringbuffer *pipe, struct task_control_block *task);
int mq_writable (struct pipe_ringbuffer *pipe, struct task_control_block *task);
int fifo_write (struct pipe_ringbuffer *pipe, struct task_control_block *task);
int mq_write (struct pipe_ringbuffer *pipe, struct task_control_block *task);
int _mknod(struct pipe_ringbuffer *pipe, int dev);

#endif
