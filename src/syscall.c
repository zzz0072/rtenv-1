#include <stddef.h>
#include "syscall_def.h"
#include "resource.h"

#define ENCLOSE_QUOTE(VAR) #VAR
#define TO_STR(VAR) ENCLOSE_QUOTE(VAR)

#define SYS_CALL_BODY(ACT) \
    __asm__( \
      "push {r7}\n" \
      "mov r7," ACT "\n"\
      "svc 0\n"\
      "nop\n"\
      "pop {r7}\n"\
      "bx lr\n"\
        :::\
    );


int fork(const void *proc_descption) __attribute__ ((naked));
int fork(const void *proc_descption)
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_FORK));
}

int getpid() __attribute__ ((naked));
int getpid()
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_GETTID));
}

int write(int fd, const void *buf, size_t count) __attribute__ ((naked));
int write(int fd, const void *buf, size_t count)
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_WRITE));
}

int read(int fd, void *buf, size_t count) __attribute__ ((naked));
int read(int fd, void *buf, size_t count)
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_READ));
}

void interrupt_wait(int intr) __attribute__ ((naked));
void interrupt_wait(int intr)
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_WAIT_INTR));
}

int getpriority(int who) __attribute__ ((naked));
int getpriority(int who)
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_GETPRIORITY));
}

int setpriority(int who, int value) __attribute__ ((naked));
int setpriority(int who, int value)
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_SETPRIORITY));
}

int mknod(int fd, int mode, int dev) __attribute__ ((naked));
int mknod(int fd, int mode, int dev)
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_MK_NODE));
}

void sleep(unsigned int msec) __attribute__ ((naked));
void sleep(unsigned int msec)
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_SLEEP));
}

void lseek(int fd, int offset, int whence) __attribute__ ((naked));
void lseek(int fd, int offset, int whence)
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_LSEEK));
}

int setrlimit(int resource, const struct rlimit *rlp) __attribute__ ((naked));
int setrlimit(int resource, const struct rlimit *rlp)
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_SETRLIMIT));
}

void exit(int status) __attribute__ ((naked));
void exit(int status)
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_EXIT));
}

int waitpid(int pid, int *status, int options) __attribute__ ((naked));
int waitpid(int pid, int *status, int options)
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_WAITTID));
}

void *mmap(void *addr, size_t len, int prot, int flags, int fd, int off) __attribute__ ((naked));
void *mmap(void *addr, size_t len, int prot, int flags, int fd, int off)
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_MMAP));
}

int exec_addr(void *addr, char *const argv[], char *const envp[]) __attribute__ ((naked));
int exec_addr(void *addr, char *const argv[], char *const envp[])

{
    SYS_CALL_BODY(TO_STR(SYS_CALL_EXEC_ADDR));
}

int rmnod(int fd) __attribute__ ((naked));
int rmnod(int fd)
{
    SYS_CALL_BODY(TO_STR(SYS_CALL_RMNOD));
}
