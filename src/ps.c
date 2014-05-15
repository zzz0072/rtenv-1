#include <string.h>
#include "program.h"
#include "syscall.h"
#include "file.h"
#include "string.h"


#define PS_PATH_LEN 16

int ps(int argc, char *argv[]);

PROGRAM_DECLARE(ps, ps);

int ps(int argc, char *argv[])
{
    char ps_message[] = "TID STATUS PRIORITY";
    char proc_path[PS_PATH_LEN];
    int proc_file;
    int task_i;
    int tid;
    int status;
    int priority;

    printf("%s\n\r", ps_message);

    for (task_i = 0; task_i < TASK_LIMIT; task_i++) {
        sprintf(proc_path, "/proc/%d/stat", task_i);
        proc_file = open(proc_path, 0);

        if (proc_file != -1) {
            lseek(proc_file, 0, SEEK_SET);
            if (read(proc_file, &tid, sizeof(tid)) == -1)
                continue;
            if (read(proc_file, &status, sizeof(status)) == -1)
                continue;
            if (read(proc_file, &priority, sizeof(priority)) == -1)
                continue;

            printf("%d\t%d\t%d\n\r", tid, status, priority);

            close(proc_file);
        }
    }

    return 0;
}
