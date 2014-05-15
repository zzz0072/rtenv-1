#ifndef SYSCALL_DEF_H_20130919
#define SYSCALL_DEF_H_20130919

/* System calls */
#define SYS_CALL_FORK        (0x01)
#define SYS_CALL_GETTID      (0x02)
#define SYS_CALL_WRITE       (0x03)
#define SYS_CALL_READ        (0x04)
#define SYS_CALL_WAIT_INTR   (0x05)
#define SYS_CALL_GETPRIORITY (0x06)
#define SYS_CALL_SETPRIORITY (0x07)
#define SYS_CALL_MK_NODE     (0x08)
#define SYS_CALL_SLEEP       (0x09)
#define SYS_CALL_LSEEK       (0x0A)
#define SYS_CALL_SETRLIMIT   (0x0B)
#define SYS_CALL_RMNOD       (0x0C)
#define SYS_CALL_EXIT        (0x0D)
#define SYS_CALL_WAITTID     (0x0E)
#define SYS_CALL_MMAP        (0x0F)
#define SYS_CALL_EXEC_ADDR   (0x10)

#endif /* SYSCALL_DEF_H_20130919 */
