#include "shell.h"

#include <stddef.h>
#include "mqueue.h"
#include "syscall.h"
#include <ctype.h>
#include "string.h"
#include "program.h"
#include "stdlib.h"

#define MAX_CMDNAME 19
#define MAX_ARGC 19
#define MAX_CMDHELP 1023
#define HISTORY_COUNT 8
#define CMDBUF_SIZE 64
#define MAX_ENVCOUNT 16
#define MAX_ENVNAME 15
#define MAX_ENVVALUE 63

/*Global Variables*/
char next_line[3] = {'\n', '\r', '\0'};
char cmd[HISTORY_COUNT][CMDBUF_SIZE];
int cur_his = 0;
int fdout;
int fdin;

void check_keyword();
void find_events();
int fill_arg(char *const dest, const char *argv);
void write_blank(int blank_num);
void serial_test_task();
void shell_init();

/* Command handlers. */
void export_envvar(int argc, char *argv[]);
void show_echo(int argc, char *argv[]);
void show_cmd_info(int argc, char *argv[]);
void show_task_info(int argc, char *argv[]);
void show_man_page(int argc, char *argv[]);
void show_history(int argc, char *argv[]);
void show_xxd(int argc, char *argv[]);

/* Enumeration for command types. */
enum {
    CMD_EXPORT,
    CMD_HELP,
    CMD_HISTORY,
    CMD_MAN,
    CMD_COUNT
} CMD_TYPE;
/* Structure for command handler. */
typedef struct {
    char cmd[MAX_CMDNAME + 1];
    void (*func)(int, char **);
    char description[MAX_CMDHELP + 1];
} hcmd_entry;
const hcmd_entry cmd_data[CMD_COUNT] = {
    [CMD_EXPORT] = {.cmd = "export", .func = export_envvar, .description = "Export environment variables."},
    [CMD_HELP] = {.cmd = "help", .func = show_cmd_info, .description = "List all commands you can use."},
    [CMD_HISTORY] = {.cmd = "history", .func = show_history, .description = "Show latest commands entered."},
    [CMD_MAN] = {.cmd = "man", .func = show_man_page, .description = "Manual pager."},
};

/* Structure for environment variables. */
typedef struct {
    char name[MAX_ENVNAME + 1];
    char value[MAX_ENVVALUE + 1];
} evar_entry;
evar_entry env_var[MAX_ENVCOUNT];
int env_count = 0;

PROGRAM_DECLARE(shell, shell_init);

void shell_init()
{
    struct rlimit rlimit = {
        .rlim_cur = 512 * 4
    };

    setrlimit(RLIMIT_STACK, &rlimit);
    serial_test_task();
}

void serial_test_task()
{
    char put_ch[2] = {'0', '\0'};
    char hint[] =  USER_NAME "@" USER_NAME "-STM32:~$ ";
    int hint_length = sizeof(hint);
    char *p = NULL;

    while ((fdout = mq_open("/tmp/mqueue/out", 0)) < 0)
        sleep(1);
    while ((fdin = open("/dev/tty0/in", 0)) < 0)
        sleep(1);

    for (;; cur_his = (cur_his + 1) % HISTORY_COUNT) {
        p = cmd[cur_his];
        write(fdout, hint, hint_length);

        while (1) {
            read(fdin, put_ch, 1);

            if (put_ch[0] == '\r' || put_ch[0] == '\n') {
                *p = '\0';
                write(fdout, next_line, 3);
                break;
            }
            else if (put_ch[0] == 127 || put_ch[0] == '\b') {
                if (p > cmd[cur_his]) {
                    p--;
                    write(fdout, "\b \b", 4);
                }
            }
            else if (p - cmd[cur_his] < CMDBUF_SIZE - 1) {
                *p++ = put_ch[0];
                write(fdout, put_ch, 2);
            }
        }
        check_keyword();
    }
}

/* Split command into tokens. */
char *cmdtok(char *cmd)
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
            else if (isspace((int)*end) && !quo)
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

void check_keyword()
{
    char *argv[1 + MAX_ARGC + 1] = {NULL};
    char cmdstr[CMDBUF_SIZE];
    int argc = 1;
    int i;
    char path[PATH_MAX + 1];

    find_events();
    fill_arg(cmdstr, cmd[cur_his]);
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
    argv[argc] = NULL;

    for (i = 0; i < CMD_COUNT; i++) {
        if (!strcmp(argv[0], cmd_data[i].cmd)) {
            cmd_data[i].func(argc, argv);
            break;
        }
    }
    if (i != CMD_COUNT) {
        return;
    }

    int child = fork();

    if (child < 0) {
        write(fdout, "No more thread can be allocated", 33);
        write(fdout, next_line, 3);
    }
    else if (child) {
        int status;
        waittid(child, &status, 0);
    }
    else {
        /* Try full path */
        execvpe(argv[0], argv, NULL);

        /* Try /bin/ */
        strcpy(path, "/bin/");
        strcpy(path + 5, argv[0]);
        execvpe(path, argv, NULL);

        /* Error */
        write(fdout, argv[0], strlen(argv[0]) + 1);
        write(fdout, ": command not found", 20);
        write(fdout, next_line, 3);
        exit(-1);
    }
}

void find_events()
{
    char buf[CMDBUF_SIZE];
    char *p = cmd[cur_his];
    char *q;
    int i;

    for (; *p; p++) {
        if (*p == '!') {
            q = p;
            while (*q && !isspace((int)*q))
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

char *find_envvar(const char *name)
{
    int i;

    for (i = 0; i < env_count; i++) {
        if (!strcmp(env_var[i].name, name))
            return env_var[i].value;
    }

    return NULL;
}

/* Fill in entire value of argument. */
int fill_arg(char *const dest, const char *argv)
{
    char env_name[MAX_ENVNAME + 1];
    char *buf = dest;
    char *p = NULL;

    for (; *argv; argv++) {
        if (isalnum((int)*argv) || *argv == '_') {
            if (p)
                *p++ = *argv;
            else
                *buf++ = *argv;
        }
        else {   /* Symbols. */
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

//export
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

//help

void show_cmd_info(int argc, char *argv[])
{
    const char help_desp[] = "This system has commands as follow\n\r\0";
    int i;

    write(fdout, &help_desp, sizeof(help_desp));
    for (i = 0; i < CMD_COUNT; i++) {
        write(fdout, cmd_data[i].cmd, strlen(cmd_data[i].cmd) + 1);
        write(fdout, ": ", 3);
        write(fdout, cmd_data[i].description, strlen(cmd_data[i].description) + 1);
        write(fdout, next_line, 3);
    }
}

//man
void show_man_page(int argc, char *argv[])
{
    int i;

    if (argc < 2)
        return;

    for (i = 0; i < CMD_COUNT && strcmp(cmd_data[i].cmd, argv[1]); i++)
        ;

    if (i >= CMD_COUNT)
        return;

    write(fdout, "NAME: ", 7);
    write(fdout, cmd_data[i].cmd, strlen(cmd_data[i].cmd) + 1);
    write(fdout, next_line, 3);
    write(fdout, "DESCRIPTION: ", 14);
    write(fdout, cmd_data[i].description, strlen(cmd_data[i].description) + 1);
    write(fdout, next_line, 3);
}

void show_history(int argc, char *argv[])
{
    int i;

    for (i = cur_his + 1; i <= cur_his + HISTORY_COUNT; i++) {
        if (cmd[i % HISTORY_COUNT][0]) {
            write(fdout, cmd[i % HISTORY_COUNT], strlen(cmd[i % HISTORY_COUNT]) + 1);
            write(fdout, next_line, 3);
        }
    }
}

void write_blank(int blank_num)
{
    char blank[] = " ";
    int blank_count = 0;

    while (blank_count <= blank_num) {
        write(fdout, blank, sizeof(blank));
        blank_count++;
    }
}

