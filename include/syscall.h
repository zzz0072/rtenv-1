#include <stddef.h>
#include "resource.h"

void *activate(void *stack);

int fork();
int gettid();

int write(int fd, const void *buf, size_t count);
int read(int fd, void *buf, size_t count);

void interrupt_wait(int intr);

int getpriority(int who);
int setpriority(int who, int value);

int mknod(int fd, int mode, int dev);
int rmnod(int fd);

void sleep(unsigned int);

void lseek(int fd, int offset, int whence);

int setrlimit(int resource, const struct rlimit *rlp);

void exit(int status);
int waittid(int tid, int *status, int options);

void *mmap(void *addr, size_t len, int prot, int flags, int fd, int off);

int exec_addr(void *addr, char *const argv[], char *const envp[]);
