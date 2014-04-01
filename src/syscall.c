#include <stddef.h>
#include "syscall_def.h"

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

int gettid() __attribute__ ((naked));
int gettid()
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

