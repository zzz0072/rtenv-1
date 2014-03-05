#ifndef SHELL_H_2012022
#define SHELL_H_2012022

#define MAX_CMDNAME 19
#define MAX_ARGC 19
#define MAX_CMDHELP 1023
#define HISTORY_COUNT 20
#define CMDBUF_SIZE 100
#define MAX_ENVCOUNT 30
#define MAX_ENVNAME 15
#define MAX_ENVVALUE 127

/* Structure for environment variables. */
typedef struct {
    char name[MAX_ENVNAME + 1];
    char value[MAX_ENVVALUE + 1];
} evar_entry;


void shell_task();
#endif
