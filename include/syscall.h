#ifndef SYSCALL_H_20130919
#define SYSCALL_H_20130919
#include <stddef.h>
#include "syscall_def.h"

void *activate(void *stack);

int fork();
int gettid();

int write(int fd, const void *buf, size_t count);
int read(int fd, void *buf, size_t count);

void interrupt_wait(int intr);

int getpriority(int who);
int setpriority(int who, int value);

int mknod(int fd, int mode, int dev);

void sleep(unsigned int);

void lseek(int fd, int offset, int whence);

#endif /* SYSCALL_H_20130919 */
