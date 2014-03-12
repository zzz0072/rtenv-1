#include "proc.h"
#include "task.h"
#include "syscall.h"
#include "rt_string.h"
#include "path_server.h"

void proc_task(void)
{
    int fd;
    int status = 0;

    mkfifo("/dev/tty1/out", 0);
    fd =  open("/dev/tty1/out", 0); 

    printf("read:%d\n\r", fd);
    while(1) {
        status = get_fd_status(fd);

        if (status & REQ_RD) {
            printf("read:%d\n\r", status);
        }
    }
}
void test_task(void)
{
    int fd = open("/dev/tty1/out", 0);
    int status = 0;

    read(fd, &status, 4);
    while(1) ;
}
