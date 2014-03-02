#include <stddef.h>
#include <ctype.h>
#include <string.h>
#include "path_server.h"
#include "rt_string.h"
#include "syscall.h"
#include "malloc.h"

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

#define BACKSPACE (127)
#define ESC        (27)
#define SPACE      (32)

#define RT_NO  (0)
#define RT_YES (1)

/* Command handlers. */
void export_env_var(int argc, char *argv[]);
void show_echo(int argc, char *argv[]);
void show_cmd_info(int argc, char *argv[]);
void show_task_info(int argc, char *argv[]);
void show_man_page(int argc, char *argv[]);
void show_history(int argc, char *argv[]);

/**************************
 * Internal data structures
***************************/
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

static char g_cmd_hist[HISTORY_COUNT][CMDBUF_SIZE];
static int g_cur_cmd_hist_pos=0;
static int g_env_var_count = 0;

static const hcmd_entry g_available_cmds[] = {
    {.cmd = "echo", .func = show_echo, .description = "Show words you input."},
    {.cmd = "export", .func = export_env_var, .description = "Export environment variables."},
    {.cmd = "help", .func = show_cmd_info, .description = "List all commands you can use."},
    {.cmd = "history", .func = show_history, .description = "Show latest commands entered."},
    {.cmd = "man", .func = show_man_page, .description = "Manual pager."},
    {.cmd = "ps", .func = show_task_info, .description = "List all the processes."}
};

static evar_entry env_var[MAX_ENVCOUNT];

#define CMD_COUNT (sizeof(g_available_cmds)/sizeof(hcmd_entry))
/************************
 * Internal functions
*************************/
static void hist_expand()
{
    char buf[CMDBUF_SIZE];
    char *p = g_cmd_hist[g_cur_cmd_hist_pos];
    char *q;
    int i;

    /* ex: 'help' in g_cmd_hist[] can be run in !h, !he, !hel at command line */
    for (; *p; p++) {
        if (*p != '!') {
            continue;
        }

        q = p;
        while (*q && !isspace((unsigned char)*q)) {
            q++;
        }

        for (i = g_cur_cmd_hist_pos + HISTORY_COUNT - 1; i > g_cur_cmd_hist_pos; i--) {
            if (!strncmp(g_cmd_hist[i % HISTORY_COUNT], p + 1, q - p - 1)) {
                strcpy(buf, q);
                strcpy(p, g_cmd_hist[i % HISTORY_COUNT]);
                p += strlen(p);
                strcpy(p--, buf);
                return;
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

static char *get_env_var_val(const char *name)
{
    int i;

    for (i = 0; i < g_env_var_count; i++) {
        if (!strcmp(env_var[i].name, name))
            return env_var[i].value;
    }

    return NULL;
}

/* Fill in entire value of argument. */
static int env_var_expand(char *const dest, const char *argv)
{
    char env_var_name[MAX_ENVNAME + 1];
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
                p = get_env_var_val(env_var_name);
                if (p) {
                    strcpy(buf, p);
                    buf += strlen(p);
                    p = NULL;
                }
            }
            if (*argv == '$')
                p = env_var_name;
            else
                *buf++ = *argv;
        }
    }
    if (p) {
        *p = '\0';
        p = get_env_var_val(env_var_name);
        if (p) {
            strcpy(buf, p);
            buf += strlen(p);
        }
    }
    *buf = '\0';

    return buf - dest;
}

static void run_cmd()
{
    char *argv[MAX_ARGC + 1] = {NULL};
    char cmdstr[CMDBUF_SIZE];
    char buffer[CMDBUF_SIZE * MAX_ENVVALUE / 2 + 1];
    char *p = buffer;
    int argc = 1;
    int i;

    hist_expand();
    strcpy(cmdstr, g_cmd_hist[g_cur_cmd_hist_pos]);
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
        int l = env_var_expand(p, argv[i]);
        argv[i] = p;
        p += l + 1;
    }

    for (i = 0; i < CMD_COUNT; i++) {
        if (!strcmp(argv[0], g_available_cmds[i].cmd)) {
            g_available_cmds[i].func(argc, argv);
            break;
        }
    }
    if (i == CMD_COUNT) {
        printf("%s: command not found\n\r", argv[0]);
    }
}


static char *readline(char *prompt)
{
    int fdin;
    char ch[] = {0x00, 0x00};
    char last_char_is_ESC = RT_NO;
    int curr_char;
    char *read_buf = (char *)malloc(CMDBUF_SIZE);

    if (read_buf == 0) {
        return 0;
    }

    fdin = open("/dev/tty0/in", 0);
    curr_char = 0;

    printf("%s", prompt);
    while(1) {
        /* Receive a byte from the RS232 port (this call will
         * block). */
        read(fdin, &ch[0], 1);

        /* Handle ESC case first */
        if (last_char_is_ESC == RT_YES) {
            last_char_is_ESC = RT_NO;

            if (ch[0] == '[') {
                /* Direction key: ESC[A ~ ESC[D */
                read(fdin, &ch[0], 1);

                /* Home:      ESC[1~
                 * End:       ESC[2~
                 * Insert:    ESC[3~
                 * Delete:    ESC[4~
                 * Page up:   ESC[5~
                 * Page down: ESC[6~ */
                if (ch[0] >= '1' && ch[0] <= '6') {
                    read(fdin, &ch[0], 1);
                }
                continue;
            }
        }

        /* If the byte is an end-of-line type character, then
         * finish the string and inidcate we are done.
         */
        if (curr_char == (CMDBUF_SIZE - 2) || \
            (ch[0] == '\r') || (ch[0] == '\n')) {
            *(read_buf + curr_char) = '\n';
            *(read_buf + curr_char + 1) = '\0';
            break;
        }
        else if(ch[0] == ESC) {
            last_char_is_ESC = RT_YES;
        }
        /* Skip control characters. man ascii for more information */
        else if (ch[0] < 0x20) {
            continue;
        }
        else if(ch[0] == BACKSPACE) { /* backspace */
            if(curr_char > 0) {
                curr_char--;
                printf("\b \b");
            }
        }
        else {
            /* Appends only when buffer is not full.
             * Include \n\0 */
            if (curr_char < (CMDBUF_SIZE - 3)) {
                *(read_buf + curr_char++) = ch[0];
                puts(ch);
            }
        }
    }
    printf("\n\r");

    return read_buf;
}


/************************
 * Command handlers
*************************/
/* export */
void export_env_var(int argc, char *argv[])
{
    char *env_var_val;
    char *value;
    int i;

    for (i = 1; i < argc; i++) {
        value = argv[i];
        while (*value && *value != '=')
            value++;
        if (*value)
            *value++ = '\0';
        env_var_val = get_env_var_val(argv[i]);
        if (env_var_val)
            strcpy(env_var_val, value);
        else if (g_env_var_count < MAX_ENVCOUNT) {
            strcpy(env_var[g_env_var_count].name, argv[i]);
            strcpy(env_var[g_env_var_count].value, value);
            g_env_var_count++;
        }
    }
}

/* ps */
void show_task_info(int argc, char* argv[])
{
    char ps_message[] = "TID\tSTATUS\tPRIORITY";
    char *str_to_output = 0;
    int task_i;

    printf("%s\n\r", ps_message);

    for (task_i = 0; task_i < task_count; task_i++) {
        char task_info_tid[2];
        char task_info_status[2];
        char task_info_priority[MAX_ITOA_CHARS];

        task_info_tid[0]='0'+tasks[task_i].tid;
        task_info_tid[1]='\0';
        task_info_status[0]='0'+tasks[task_i].status;
        task_info_status[1]='\0';

        str_to_output = itoa(tasks[task_i].priority, task_info_priority);

        printf("%s\t%s\t%s\n\r", task_info_tid, task_info_status, str_to_output);
    }
}

/* help */
void show_cmd_info(int argc, char* argv[])
{
    int i;

    printf("This system has commands as follow\n\r");
    for (i = 0; i < CMD_COUNT; i++) {
        printf("%s: %s\n\r", g_available_cmds[i].cmd, g_available_cmds[i].description);
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

    for (i = 0; i < CMD_COUNT && strcmp(g_available_cmds[i].cmd, argv[1]); i++)
        ;

    if (i >= CMD_COUNT)
        return;

    printf("NAME: %s\n\rDESCRIPTION:%s \n\r", g_available_cmds[i].cmd, g_available_cmds[i].description);
}

void show_history(int argc, char *argv[])
{
    int i;

    for (i = g_cur_cmd_hist_pos + 1; i <= g_cur_cmd_hist_pos + HISTORY_COUNT; i++) {
        if (g_cmd_hist[i % HISTORY_COUNT][0]) {
            printf("%s\n\r", g_cmd_hist[i % HISTORY_COUNT]);
        }
    }
}

/**************************
 * task to handle commands
***************************/
void shell_task()
{
    char *read_str = 0;

    for (;; g_cur_cmd_hist_pos = (g_cur_cmd_hist_pos + 1) % HISTORY_COUNT) {
        read_str = readline(PROMPT);
        if (!read_str) {
            continue;
        }

        strncpy(g_cmd_hist[g_cur_cmd_hist_pos], read_str, CMDBUF_SIZE);
        run_cmd(); /* cmd string passed by g_cmd_hist */
        free(read_str);
        read_str = 0;
    }
}

