#ifndef SYSCALL_H_20130919
#define SYSCALL_H_20130919
#include <stddef.h>
#include "syscall_def.h"

void *activate(void *stack);

int fork();
int getpid();

int write(int fd, const void *buf, size_t count);
int read(int fd, void *buf, size_t count);

void interrupt_wait(int intr);

int getpriority(int who);
int setpriority(int who, int value);

int mknod(int fd, int mode, int dev);

void sleep(unsigned int);

int get_fd_status(int fd);
#endif /* SYSCALL_H_20130919 */
