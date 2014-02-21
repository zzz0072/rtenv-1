#include <stddef.h>
#include <string.h>
#include "syscall.h"
#include "rt_string.h"
#include "path_server.h"
#include <ctype.h>
/* Internal defines */
#define MAX_CMDNAME 19
#define MAX_ARGC 19
#define MAX_CMDHELP 1023
#define HISTORY_COUNT 20
#define CMDBUF_SIZE 100
#define MAX_ENVCOUNT 30
#define MAX_ENVNAME 15
#define MAX_ENVVALUE 127
#define PROMPT USER_NAME "@" USER_NAME "-STM32:~$ "

/* Command handlers. */
void export_envvar(int argc, char *argv[]);
void show_echo(int argc, char *argv[]);
void show_cmd_info(int argc, char *argv[]);
void show_task_info(int argc, char *argv[]);
void show_man_page(int argc, char *argv[]);
void show_history(int argc, char *argv[]);

/**************************
 * Internal data structures
***************************/
/* Enumeration for command types. */
enum {
    CMD_ECHO = 0,
    CMD_EXPORT,
    CMD_HELP,
    CMD_HISTORY,
    CMD_MAN,
    CMD_PS,
    CMD_COUNT
} CMD_TYPE;

/* Structure for command handler. */
typedef struct {
    char cmd[MAX_CMDNAME + 1];
    void (*func)(int, char**);
    char description[MAX_CMDHELP + 1];
} hcmd_entry;

/* Structure for environment variables. */
typedef struct {
    char name[MAX_ENVNAME + 1];
    char value[MAX_ENVVALUE + 1];
} evar_entry;


/************************
 * Global variables
*************************/
extern size_t task_count;
extern struct task_control_block tasks[TASK_LIMIT];

static char cmd[HISTORY_COUNT][CMDBUF_SIZE];
static int cur_his=0;
static int fdout;
static int fdin;
static int env_count = 0;

static const hcmd_entry cmd_data[CMD_COUNT] = {
    [CMD_ECHO] = {.cmd = "echo", .func = show_echo, .description = "Show words you input."},
    [CMD_EXPORT] = {.cmd = "export", .func = export_envvar, .description = "Export environment variables."},
    [CMD_HELP] = {.cmd = "help", .func = show_cmd_info, .description = "List all commands you can use."},
    [CMD_HISTORY] = {.cmd = "history", .func = show_history, .description = "Show latest commands entered."},
    [CMD_MAN] = {.cmd = "man", .func = show_man_page, .description = "Manual pager."},
    [CMD_PS] = {.cmd = "ps", .func = show_task_info, .description = "List all the processes."}
};

static evar_entry env_var[MAX_ENVCOUNT];

/************************
 * Internal functions
*************************/
static void find_events()
{
    char buf[CMDBUF_SIZE];
    char *p = cmd[cur_his];
    char *q;
    int i;

    for (; *p; p++) {
        if (*p == '!') {
            q = p;
            while (*q && !isspace((unsigned char)*q))
                q++;
            for (i = cur_his + HISTORY_COUNT - 1; i > cur_his; i--) {
                if (!strncmp(cmd[i % HISTORY_COUNT], p + 1, q - p - 1)) {
                    strcpy(buf, q);
                    strcpy(p, cmd[i % HISTORY_COUNT]);
                    p += strlen(p);
                    strcpy(p--, buf);
                    break;
                }
            }
        }
    }
}

/* Split command into tokens. */
static char *cmdtok(char *cmd)
{
    static char *cur = NULL;
    static char *end = NULL;
    if (cmd) {
        char quo = '\0';
        cur = cmd;
        for (end = cmd; *end; end++) {
            if (*end == '\'' || *end == '\"') {
                if (quo == *end)
                    quo = '\0';
                else if (quo == '\0')
                    quo = *end;
                *end = '\0';
            }
            else if (isspace((unsigned char)*end) && !quo)
                *end = '\0';
        }
    }
    else
        for (; *cur; cur++)
            ;

    for (; *cur == '\0'; cur++)
        if (cur == end) return NULL;
    return cur;
}

static char *find_envvar(const char *name)
{
    int i;

    for (i = 0; i < env_count; i++) {
        if (!strcmp(env_var[i].name, name))
            return env_var[i].value;
    }

    return NULL;
}

/* Fill in entire value of argument. */
static int fill_arg(char *const dest, const char *argv)
{
    char env_name[MAX_ENVNAME + 1];
    char *buf = dest;
    char *p = NULL;

    for (; *argv; argv++) {
        if (isalnum((unsigned char)*argv) || *argv == '_') {
            if (p)
                *p++ = *argv;
            else
                *buf++ = *argv;
        }
        else { /* Symbols. */
            if (p) {
                *p = '\0';
                p = find_envvar(env_name);
                if (p) {
                    strcpy(buf, p);
                    buf += strlen(p);
                    p = NULL;
                }
            }
            if (*argv == '$')
                p = env_name;
            else
                *buf++ = *argv;
        }
    }
    if (p) {
        *p = '\0';
        p = find_envvar(env_name);
        if (p) {
            strcpy(buf, p);
            buf += strlen(p);
        }
    }
    *buf = '\0';

    return buf - dest;
}

static void check_keyword()
{
    char *argv[MAX_ARGC + 1] = {NULL};
    char cmdstr[CMDBUF_SIZE];
    char buffer[CMDBUF_SIZE * MAX_ENVVALUE / 2 + 1];
    char *p = buffer;
    int argc = 1;
    int i;

    find_events();
    strcpy(cmdstr, cmd[cur_his]);
    argv[0] = cmdtok(cmdstr);
    if (!argv[0])
        return;

    while (1) {
        argv[argc] = cmdtok(NULL);
        if (!argv[argc])
            break;
        argc++;
        if (argc >= MAX_ARGC)
            break;
    }

    for(i = 0; i < argc; i++) {
        int l = fill_arg(p, argv[i]);
        argv[i] = p;
        p += l + 1;
    }

    for (i = 0; i < CMD_COUNT; i++) {
        if (!strcmp(argv[0], cmd_data[i].cmd)) {
            cmd_data[i].func(argc, argv);
            break;
        }
    }
    if (i == CMD_COUNT) {
        printf("%s: command not found\n\r", argv[0]);
    }
}

/************************
 * Command handlers
*************************/
void echo()
{
    int fdout;
    int fdin;
    char c;

    fdout = open("/dev/tty0/out", 0);
    fdin = open("/dev/tty0/in", 0);

    while (1) {
        read(fdin, &c, 1);
        write(fdout, &c, 1);
    }
}

/* export */
void export_envvar(int argc, char *argv[])
{
    char *found;
    char *value;
    int i;

    for (i = 1; i < argc; i++) {
        value = argv[i];
        while (*value && *value != '=')
            value++;
        if (*value)
            *value++ = '\0';
        found = find_envvar(argv[i]);
        if (found)
            strcpy(found, value);
        else if (env_count < MAX_ENVCOUNT) {
            strcpy(env_var[env_count].name, argv[i]);
            strcpy(env_var[env_count].value, value);
            env_count++;
        }
    }
}

/* ps */
void show_task_info(int argc, char* argv[])
{
    char ps_message[]="PID\tSTATUS\tPRIORITY";
    char *str_to_output = 0;
    int task_i;

    printf("%s\n\r", ps_message);

    for (task_i = 0; task_i < task_count; task_i++) {
        char task_info_pid[2];
        char task_info_status[2];
        char task_info_priority[MAX_ITOA_CHARS];

        task_info_pid[0]='0'+tasks[task_i].pid;
        task_info_pid[1]='\0';
        task_info_status[0]='0'+tasks[task_i].status;
        task_info_status[1]='\0';

        str_to_output = itoa(tasks[task_i].priority, task_info_priority);

        printf("%s\t%s\t%s\n\r", task_info_pid, task_info_status, str_to_output);
    }
}

/* help */

void show_cmd_info(int argc, char* argv[])
{
    int i;

    printf("This system has commands as follow\n\r");
    for (i = 0; i < CMD_COUNT; i++) {
        printf("%s: %s\n\r", cmd_data[i].cmd, cmd_data[i].description);
    }
}

/* echo */
void show_echo(int argc, char* argv[])
{
    const int _n = 1; /* Flag for "-n" option. */
    int flag = 0;
    int i;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-n"))
            flag |= _n;
        else
            break;
    }

    for (; i < argc; i++) {
        printf("%s", argv[i]);
        if (i < argc - 1)
            printf(" ");
    }

    if (~flag & _n)
        printf("\n\r");
}

/* man */
void show_man_page(int argc, char *argv[])
{
    int i;

    if (argc < 2)
        return;

    for (i = 0; i < CMD_COUNT && strcmp(cmd_data[i].cmd, argv[1]); i++)
        ;

    if (i >= CMD_COUNT)
        return;

    printf("NAME: %s\n\rDESCRIPTION:%s \n\r", cmd_data[i].cmd, cmd_data[i].description);
}

void show_history(int argc, char *argv[])
{
    int i;

    for (i = cur_his + 1; i <= cur_his + HISTORY_COUNT; i++) {
        if (cmd[i % HISTORY_COUNT][0]) {
            printf("%s\n\r", cmd[i % HISTORY_COUNT]);
        }
    }
}

/**************************
 * task to handle commands
***************************/
void shell_task()
{
    char put_ch[2]={'0','\0'};
    char *p = NULL;

    fdout = mq_open("/tmp/mqueue/out", 0);
    fdin = open("/dev/tty0/in", 0);

    for (;; cur_his = (cur_his + 1) % HISTORY_COUNT) {
        p = cmd[cur_his];
        printf(PROMPT);

        while (1) {
            read(fdin, put_ch, 1);

            if (put_ch[0] == '\r' || put_ch[0] == '\n') {
                *p = '\0';
                printf("\n\r");
                break;
            }
            else if (put_ch[0] == 127 || put_ch[0] == '\b') {
                if (p > cmd[cur_his]) {
                    p--;
                    printf("\b \b");
                }
            }
            else if (p - cmd[cur_his] < CMDBUF_SIZE - 1) {
                *p++ = put_ch[0];
                printf("%s", put_ch);
            }
        }
        check_keyword();
    }
}


