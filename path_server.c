#include "path_server.h"
#include "rt_string.h"
#include "syscall.h"
/*
 * pathserver assumes that all files are FIFOs that were registered
 * with mkfifo.  It also assumes a global tables of FDs shared by all
 * processes.  It would have to get much smarter to be generally useful.
 *
 * The first TASK_LIMIT FDs are reserved for use by their respective tasks.
 * 0-2 are reserved FDs and are skipped.
 * The server registers itself at /sys/pathserver
*/
#define PATH_SERVER_NAME "/sys/pathserver"
void pathserver()
{
    char paths[PIPE_LIMIT - TASK_LIMIT - 3][PATH_MAX];
    int npaths = 0;
    int i = 0;
    unsigned int plen = 0;
    unsigned int replyfd = 0;
    char path[PATH_MAX];

    memcpy(paths[npaths++], PATH_SERVER_NAME, sizeof(PATH_SERVER_NAME));

    while (1) {
        read(PATHSERVER_FD, &replyfd, 4);
        read(PATHSERVER_FD, &plen, 4);
        read(PATHSERVER_FD, path, plen);

        if (!replyfd) { /* mkfifo */
            int dev;
            read(PATHSERVER_FD, &dev, 4);
            memcpy(paths[npaths], path, plen);
            mknod(npaths + 3 + TASK_LIMIT, 0, dev);
            npaths++;
        }
        else { /* open */
            /* Search for path */
            for (i = 0; i < npaths; i++) {
                if (*paths[i] && strcmp(path, paths[i]) == 0) {
                    i += 3; /* 0-2 are reserved */
                    i += TASK_LIMIT; /* FDs reserved for tasks */
                    write(replyfd, &i, 4);
                    i = 0;
                    break;
                }
            }

            if (i >= npaths) {
                i = -1; /* Error: not found */
                write(replyfd, &i, 4);
            }
        }
    }
}

int mkfile(const char *pathname, int mode, int dev)
{
    size_t plen = strlen(pathname)+1;
    char buf[4+4+PATH_MAX+4];
    (void) mode;

    *((unsigned int *)buf) = 0;
    *((unsigned int *)(buf + 4)) = plen;
    memcpy(buf + 4 + 4, pathname, plen);
    *((int *)(buf + 4 + 4 + plen)) = dev;
    write(PATHSERVER_FD, buf, 4 + 4 + plen + 4);

    return 0;
}

int mkfifo(const char *pathname, int mode)
{
    mkfile(pathname, mode, S_IFIFO);
    return 0;
}

int open(const char *pathname, int flags)
{
    unsigned int replyfd = getpid() + 3;
    size_t plen = strlen(pathname) + 1;
    unsigned int fd = -1;
    char buf[4 + 4 + PATH_MAX];
    (void) flags;

    *((unsigned int *)buf) = replyfd;
    *((unsigned int *)(buf + 4)) = plen;
    memcpy(buf + 4 + 4, pathname, plen);
    write(PATHSERVER_FD, buf, 4 + 4 + plen);
    read(replyfd, &fd, 4);

    return fd;
}

int mq_open(const char *name, int oflag)
{
    if (oflag & O_CREAT)
        mkfile(name, 0, S_IMSGQ);
    return open(name, 0);
}

#define RB_PUSH(rb, size, v) do { \
        (rb).data[(rb).end] = (v); \
        (rb).end++; \
        if ((rb).end >= size) (rb).end = 0; \
    } while (0)

#define RB_POP(rb, size, v) do { \
        (v) = (rb).data[(rb).start]; \
        (rb).start++; \
        if ((rb).start >= size) (rb).start = 0; \
    } while (0)

#define RB_PEEK(rb, size, v, i) do { \
        int _counter = (i); \
        int _src_index = (rb).start; \
        int _dst_index = 0; \
        while (_counter--) { \
            ((char*)&(v))[_dst_index++] = (rb).data[_src_index++]; \
            if (_src_index >= size) _src_index = 0; \
        } \
    } while (0)

#define RB_LEN(rb, size) (((rb).end - (rb).start) + \
    (((rb).end < (rb).start) ? size : 0))

#define PIPE_PUSH(pipe, v) RB_PUSH((pipe), PIPE_BUF, (v))
#define PIPE_POP(pipe, v)  RB_POP((pipe), PIPE_BUF, (v))
#define PIPE_PEEK(pipe, v, i)  RB_PEEK((pipe), PIPE_BUF, (v), (i))
#define PIPE_LEN(pipe)     (RB_LEN((pipe), PIPE_BUF))
void _read(struct task_control_block *task, struct task_control_block *tasks, size_t task_count, struct pipe_ringbuffer *pipes)
{
    task->status = TASK_READY;
    /* If the fd is invalid */
    if (task->stack->r0 > PIPE_LIMIT) {
        task->stack->r0 = -1;
    }
    else {
        struct pipe_ringbuffer *pipe = &pipes[task->stack->r0];

        pipe->req_st = pipe->req_st | REQ_RD;
        if (pipe->readable(pipe, task)) {
            size_t i;

            pipe->read(pipe, task);
            pipe->req_st = pipe->req_st & !REQ_RD;
            /* Unblock any waiting writes */
            for (i = 0; i < task_count; i++)
                if (tasks[i].status == TASK_WAIT_WRITE)
                    _write(&tasks[i], tasks, task_count, pipes);
        }
    }
}

void _write(struct task_control_block *task, struct task_control_block *tasks, size_t task_count, struct pipe_ringbuffer *pipes)
{
    task->status = TASK_READY;
    /* If the fd is invalid */
    if (task->stack->r0 > PIPE_LIMIT) {
        task->stack->r0 = -1;
    }
    else {
        struct pipe_ringbuffer *pipe = &pipes[task->stack->r0];

        pipe->req_st = pipe->req_st | REQ_WR;
        if (pipe->writable(pipe, task)) {
            size_t i;

            pipe->write(pipe, task);

            pipe->req_st = pipe->req_st & !REQ_WR;
            /* Unblock any waiting reads */
            for (i = 0; i < task_count; i++)
                if (tasks[i].status == TASK_WAIT_READ)
                    _read(&tasks[i], tasks, task_count, pipes);
        }
    }
}

int fifo_readable (struct pipe_ringbuffer *pipe, struct task_control_block *task)
{
    /* Trying to read too much */
    if (task->stack->r2 > PIPE_BUF) {
        task->stack->r0 = -1;
        return 0;
    }
    if ((size_t)PIPE_LEN(*pipe) < task->stack->r2) {
        /* Trying to read more than there is: block */
        task->status = TASK_WAIT_READ;
        return 0;
    }
    return 1;
}

int mq_readable (struct pipe_ringbuffer *pipe, struct task_control_block *task)
{
    size_t msg_len;

    /* Trying to read too much */
    if ((size_t)PIPE_LEN(*pipe) < sizeof(size_t)) {
        /* Nothing to read */
        task->status = TASK_WAIT_READ;
        return 0;
    }

    PIPE_PEEK(*pipe, msg_len, 4);

    if (msg_len > task->stack->r2) {
        /* Trying to read more than buffer size */
        task->stack->r0 = -1;
        return 0;
    }
    return 1;
}

int fifo_read (struct pipe_ringbuffer *pipe, struct task_control_block *task)
{
    size_t i;
    char *buf = (char*)task->stack->r1;
    /* Copy data into buf */
    for (i = 0; i < task->stack->r2; i++) {
        PIPE_POP(*pipe, buf[i]);
    }
    return task->stack->r2;
}

int mq_read (struct pipe_ringbuffer *pipe, struct task_control_block *task)
{
    size_t msg_len;
    size_t i;
    char *buf = (char*)task->stack->r1;
    /* Get length */
    for (i = 0; i < 4; i++) {
        PIPE_POP(*pipe, *(((char*)&msg_len)+i));
    }
    /* Copy data into buf */
    for (i = 0; i < msg_len; i++) {
        PIPE_POP(*pipe, buf[i]);
    }
    return msg_len;
}

int fifo_writable (struct pipe_ringbuffer *pipe, struct task_control_block *task)
{
    /* If the write would be non-atomic */
    if (task->stack->r2 > PIPE_BUF) {
        task->stack->r0 = -1;
        return 0;
    }
    /* Preserve 1 byte to distiguish empty or full */
    if ((size_t)PIPE_BUF - PIPE_LEN(*pipe) - 1 < task->stack->r2) {
        /* Trying to write more than we have space for: block */
        task->status = TASK_WAIT_WRITE;
        return 0;
    }
    return 1;
}

int mq_writable (struct pipe_ringbuffer *pipe, struct task_control_block *task)
{
    size_t total_len = sizeof(size_t) + task->stack->r2;

    /* If the write would be non-atomic */
    if (total_len > PIPE_BUF) {
        task->stack->r0 = -1;
        return 0;
    }
    /* Preserve 1 byte to distiguish empty or full */
    if ((size_t)PIPE_BUF - PIPE_LEN(*pipe) - 1 < total_len) {
        /* Trying to write more than we have space for: block */
        task->status = TASK_WAIT_WRITE;
        return 0;
    }
    return 1;
}

int fifo_write (struct pipe_ringbuffer *pipe, struct task_control_block *task)
{
    size_t i;
    const char *buf = (const char*)task->stack->r1;
    /* Copy data into pipe */
    for (i = 0; i < task->stack->r2; i++)
        PIPE_PUSH(*pipe,buf[i]);
    return task->stack->r2;
}

int mq_write (struct pipe_ringbuffer *pipe, struct task_control_block *task)
{
    size_t i;
    const char *buf = (const char*)task->stack->r1;
    /* Copy count into pipe */
    for (i = 0; i < sizeof(size_t); i++)
        PIPE_PUSH(*pipe,*(((char*)&task->stack->r2)+i));
    /* Copy data into pipe */
    for (i = 0; i < task->stack->r2; i++)
        PIPE_PUSH(*pipe,buf[i]);
    return task->stack->r2;
}

int _mknod(struct pipe_ringbuffer *pipe, int dev)
{
    switch(dev) {
    case S_IFIFO:
        pipe->readable = fifo_readable;
        pipe->writable = fifo_writable;
        pipe->read = fifo_read;
        pipe->write = fifo_write;
        break;
    case S_IMSGQ:
        pipe->readable = mq_readable;
        pipe->writable = mq_writable;
        pipe->read = mq_read;
        pipe->write = mq_write;
        break;
    default:
        return 1;
    }
    return 0;
}


