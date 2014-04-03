#include <stddef.h>
#include <ctype.h>
#include <string.h>
#include "file.h"
#include "rt_string.h"
#include "syscall.h"
#include "malloc.h"
#include "shell.h"

/* Internal defines */
#define PROMPT USER_NAME "@" USER_NAME "-STM32:"

#define BACKSPACE (127)
#define ESC        (27)
#define SPACE      (32)

#define RT_NO  (0)
#define RT_YES (1)

/* Command handlers. */
void cmd_export(int argc, char *argv[]);
void cmd_echo(int argc, char *argv[]);
void cmd_help(int argc, char *argv[]);
void cmd_ps(int argc, char *argv[]);
void cmd_man(int argc, char *argv[]);
void cmd_history(int argc, char *argv[]);
void cmd_cat(int argc, char *argv[]);
void cmd_ls(int argc, char *argv[]);
void cmd_cd(int argc, char *argv[]);

/**************************
 * Internal data structures
***************************/
/* Structure for command handler. */
typedef struct {
    char cmd[MAX_CMDNAME + 1];
    void (*func)(int, char**);
    char desc[MAX_CMDHELP + 1];
} hcmd_entry;

#define ADD_CMD(cmd_name, msg) \
    {.cmd = #cmd_name, .func=cmd_##cmd_name, .desc=msg}

/************************
 * Global variables
*************************/
extern size_t g_task_count;
extern struct task_control_block g_tasks[TASK_LIMIT];

static char g_typed_cmds[HISTORY_COUNT][CMDBUF_SIZE];
static int g_cur_cmd_hist_pos=0;
static int g_env_var_count = 0;

static const hcmd_entry g_available_cmds[] = {
    ADD_CMD(history, "List commands you typed"),
    ADD_CMD(export,  "Export variables to enviorment. Usage: VAR=VALUE"),
    ADD_CMD(echo,    "echo your string"),
    ADD_CMD(help,    "List avaialbe commands"),
    ADD_CMD(man,     "manaual for commands"),
    ADD_CMD(ps,      "List task information"),
    ADD_CMD(cat,     "dump file to serial out"),
    ADD_CMD(ls,      "list file in rootfs. ex: ls or ls /"),
    ADD_CMD(cd,      "change dir, will list current working dir if no argument"),
};

static evar_entry g_env_var[MAX_ENVCOUNT];

static char g_cwd[PATH_MAX] = "/"; /* Current working directory */
#define CMD_COUNT (sizeof(g_available_cmds)/sizeof(hcmd_entry))
/************************
 * Internal functions
*************************/
static void hist_expand()
{
    char buf[CMDBUF_SIZE];
    char *p = g_typed_cmds[g_cur_cmd_hist_pos];
    char *q;
    int i;

    /* ex: 'help' in g_typed_cmds[] can be run in !h, !he, !hel at command line */
    for (; *p; p++) {
        /* loop until first ! */
        if (*p != '!') {
            continue;
        }

        /* Skip white spaces */
        q = p;
        while (*q && !isspace((unsigned char)*q)) {
            q++;
        }

        /* since we have !, so start partial comparison */
        for (i = g_cur_cmd_hist_pos + HISTORY_COUNT - 1; i > g_cur_cmd_hist_pos; i--) {
            if (!strncmp(g_typed_cmds[i % HISTORY_COUNT], p + 1, q - p - 1)) {
                strcpy(buf, q);
                strcpy(p, g_typed_cmds[i % HISTORY_COUNT]);
                p += strlen(p);
                strcpy(p--, buf);
                return;
            }
        }
    }
}

/* Split command into tokens. */
/* Kind of strtok, first call with string then null
 * parameter after first call untill cmdtok returns
 * null */
static char *cmdtok(char *cmd)
{
    static char *cur = NULL;
    static char *end = NULL;
    if (cmd) {
        char quo = '\0';
        cur = cmd;

        /* Separate tokens */
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
    else {
        /* Get next token */
        for (; *cur; cur++)
            ;
    }

    for (; *cur == '\0'; cur++)
        if (cur == end) return NULL;
    return cur;
}

static char *get_env_var_val(const char *name)
{
    int i;

    for (i = 0; i < g_env_var_count; i++) {
        if (!strcmp(g_env_var[i].name, name))
            return g_env_var[i].value;
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
    strcpy(cmdstr, g_typed_cmds[g_cur_cmd_hist_pos]);
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

static char* retrieve_hist_cmd(char key_val,
                               char *buf,
                               unsigned int buf_size,
                               char is_first_arrow_key)
{
    /* Get last command: at this pointer, g_cur_cmd_hist_pos is still empty */
    int new_cmd_hist_pos = g_cur_cmd_hist_pos - 1;
    int previous_cmd = new_cmd_hist_pos;

    /* Regular checking */
    if (g_typed_cmds[g_cur_cmd_hist_pos - 1][0] == 0 ||
            buf == 0 || buf_size == 0) {
        return 0;
    }

    /* Leverage history */
    if (key_val == 'A') { /* Up key */
        if (is_first_arrow_key == RT_NO) {
            new_cmd_hist_pos = (new_cmd_hist_pos + HISTORY_COUNT - 1) % HISTORY_COUNT;
        }

        if (g_typed_cmds[new_cmd_hist_pos][0] == 0) {
            return 0;
        }
    }
    else if (key_val == 'B') { /* Down key */
        /* Do we need to update history position? */
        if (is_first_arrow_key == RT_NO) {
            new_cmd_hist_pos = (new_cmd_hist_pos + HISTORY_COUNT + 1) % HISTORY_COUNT;
        }

        if (g_typed_cmds[new_cmd_hist_pos][0] == 0) {
            return 0;
        }
    }

    memset(buf, 0x00, buf_size);
    /* skip \n in history buffer */
    strncpy(buf, g_typed_cmds[new_cmd_hist_pos],
            strlen(g_typed_cmds[new_cmd_hist_pos]) - 1);

    if (is_first_arrow_key == RT_YES) {
        is_first_arrow_key = RT_NO;
    }

    g_cur_cmd_hist_pos = new_cmd_hist_pos + 1;

    /* previous cmmand is used to clean up unused char to fix
     * history -> ps, and console showed psstory */
    return g_typed_cmds[previous_cmd];
}


static char *readline(char *prompt)
{
    int fdin;
    char ch[] = {0x00, 0x00};
    char last_char_is_ESC = RT_NO;
    char is_first_arrow_key = RT_YES;
    int curr_char;
    size_t rval = 0;
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
        rval = read(fdin, &ch[0], 1);
        if (rval == -1) {
            printf("Read errror... \n\r");
            return 0;
        }

        /* Handle ESC case first */
        if (last_char_is_ESC == RT_YES) {
            last_char_is_ESC = RT_NO;

            if (ch[0] == '[') {
                char *previous_cmd = 0;

                /* Direction key: ESC[A ~ ESC[D */
                read(fdin, &ch[0], 1);
                if (rval == -1) {
                    printf("Read errror... \n\r");
                    return 0;
                }

                /* Repeat previous command? */
                previous_cmd = retrieve_hist_cmd(ch[0],
                                                read_buf,
                                                CMDBUF_SIZE,
                                                is_first_arrow_key);
                if (previous_cmd) {
                    int previous_cmd_len = strlen(previous_cmd);
                    int i = 0;

                    /* Update command line */
                    curr_char = strlen(read_buf);
                    printf("\r%s%s", prompt, read_buf);

                    /* Toggle is_first_arrow_key */
                    if (is_first_arrow_key == RT_YES) {
                        is_first_arrow_key = RT_NO;
                    }

                    /* Clear a line to avoid display pslp due to previous
                     * is ps and help
                     *
                     * The overhead should be acceptable based on the
                     * assumption that user key events cost should be much 
                     * less than other tasks */

                     if (curr_char < previous_cmd_len) {
                         for (i = 0; i < (previous_cmd_len - curr_char); i++) {
                             printf(" ");
                         }
                     }
                     continue;
                }

                /* Home:      ESC[1~
                 * End:       ESC[2~
                 * Insert:    ESC[3~
                 * Delete:    ESC[4~
                 * Page up:   ESC[5~
                 * Page down: ESC[6~ */
                if (ch[0] >= '1' && ch[0] <= '6') {
                    read(fdin, &ch[0], 1);
                    if (rval == -1) {
                        printf("Read errror... \n\r");
                        return 0;
                    }
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

static void cat_buf(const char *buf, int buf_size)
{
    int i = 0;

    if (buf == 0 || buf_size <= 0) {
        return;
    }

    /* Bad implementation */
    for (i = 0; i < buf_size; i++) {
        printf("%c", buf[i]);
        if (buf[i] == '\n') {
            printf("\r");
        }
    }
}
/* return 0 means path itself is a absolute path. 
 * retrun 1 means abspath was append 
 * TODO? snprintf */
static int to_abs_path(const char *path, char * abs_path)
{
    /* Append prefix if needed */
    if (path[0] != '/') {
        /* / is a special case */
        if (strlen(g_cwd) > 1) {
            sprintf(abs_path, "%s/%s", g_cwd, path);
        }
        else {
            sprintf(abs_path, "%s%s", g_cwd, path);
        }
        return 1;
    }
    return 0;
}
/************************
 * Command handlers
*************************/
void cmd_ls(int argc, char *argv[])
{
    char abs_path[PATH_MAX] = {0};
    char *list_file = g_cwd;
    int rval = 0;

    struct stat fstat;
    if (argc > 2) {
        printf("ls only suppurt at most one argument\n\r");
    }

    if (argc == 2) {
        list_file = argv[1];

        if(to_abs_path(argv[1], abs_path)) {
            list_file = abs_path;
        }
    }

    /* test stat */
    rval = stat(list_file, &fstat);
    if (rval == -1) {
        printf("Stat failed. Maybe file does not exit?\n\r");
        return;
    }

    printf("name:\t%s\n\r", fstat.name);
    printf("len:\t%d\n\r", fstat.len);
    printf("isdir:\t%d\n\r", fstat.isdir);
}

void cmd_cd(int argc, char *argv[])
{
    int rval = 0;
    char abs_path[PATH_MAX] = {0};
    char *cd_dir = abs_path;
    struct stat fstat;

    if (argc > 2) {
        printf("ls only suppurt at most one argument\n\r");
    }

    if (argc == 1) {
        printf("%s\n\r", g_cwd);
        return;
    }

    /* Append prefix if needed */
    if (to_abs_path(argv[1], abs_path) == 0) {
        cd_dir = argv[1];
    }

    /* / is a special case, it does not really exist */
    if (strcmp(cd_dir, "/")) {
        rval = stat(cd_dir, &fstat);
        if (rval == -1) {
            printf("Stat failed. Maybe file does not exit?\n\r");
            return;
        }
    }

    strncpy(g_cwd, cd_dir, PATH_MAX);
}


void cmd_cat(int argc, char *argv[])
{
    int rval = CMDBUF_SIZE;
    int fd = 0;
    char buf[CMDBUF_SIZE];
    char abs_path[PATH_MAX] = {0};
    char *cat_file = abs_path;

    /* Current only one file*/
    if (argc != 2) {
        printf("Usage: %s filename\n\r", argv[0]);
        return;
    }

    /* Convert to absolute path if needed */
    if(to_abs_path(argv[1], abs_path) == 0) {
        cat_file = argv[1];
    }

    /* Open file */
    fd = open(cat_file, 0);
    if (fd == -1) {
        printf("Open file %s failed\n\r", argv[1]);
        return;
    }

    printf("\n\r");
    /* Read file */
    lseek(fd, 0, SEEK_SET);

    while (rval != 0) {
        rval = read(fd, buf, CMDBUF_SIZE);
        if (rval == -1) {
            printf("Read file %s failed\n\r", argv[1]);
            return;
        }

        cat_buf(buf, rval);
    }
    printf("\n\r");
}

/* export */
void cmd_export(int argc, char *argv[])
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
            strcpy(g_env_var[g_env_var_count].name, argv[i]);
            strcpy(g_env_var[g_env_var_count].value, value);
            g_env_var_count++;
        }
    }
}

/* ps */
void cmd_ps(int argc, char* argv[])
{
    char ps_message[] = "TID\tSTATUS\tPRIORITY";
    char *str_to_output = 0;
    int task_i;

    printf("%s\n\r", ps_message);

    for (task_i = 0; task_i < g_task_count; task_i++) {
        char task_info_tid[2];
        char task_info_status[2];
        char task_info_priority[MAX_ITOA_CHARS];

        task_info_tid[0]='0'+g_tasks[task_i].tid;
        task_info_tid[1]='\0';
        task_info_status[0]='0'+g_tasks[task_i].status;
        task_info_status[1]='\0';

        str_to_output = itoa(g_tasks[task_i].priority, task_info_priority);

        printf("%s\t%s\t%s\n\r", task_info_tid, task_info_status, str_to_output);
    }
}

/* help */
void cmd_help(int argc, char* argv[])
{
    int i;

    printf("Available commands:\n\n\r");
    for (i = 0; i < CMD_COUNT; i++) {
        printf("%s\t<-- %s\n\r", g_available_cmds[i].cmd, g_available_cmds[i].desc);
    }

    printf("\nMore details:\n\r");
    printf("\tEnviorment variable supported:\n\r");
    printf("\tSet-> export VAR=VAL\n\r");
    printf("\tGet-> $VAR\n\n\r");

    printf("\tYou can use ! to run command in history.\n\r");
    printf("\tEx: !p to run ps if ps is in history\n\n\r");
}

/* echo */
void cmd_echo(int argc, char* argv[])
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
void cmd_man(int argc, char *argv[])
{
    int i;

    if (argc < 2)
        return;

    for (i = 0; i < CMD_COUNT && strcmp(g_available_cmds[i].cmd, argv[1]); i++)
        ;

    if (i >= CMD_COUNT)
        return;

    printf("NAME: %s\n\rDESCRIPTION:%s \n\r", g_available_cmds[i].cmd, g_available_cmds[i].desc);
}

void cmd_history(int argc, char *argv[])
{
    int i;

    for (i = g_cur_cmd_hist_pos + 1; i <= g_cur_cmd_hist_pos + HISTORY_COUNT; i++) {
        if (g_typed_cmds[i % HISTORY_COUNT][0]) {
            printf("%s\n\r", g_typed_cmds[i % HISTORY_COUNT]);
        }
    }
}

/**************************
 * task to handle commands
***************************/
void shell_task()
{
    char *read_str = 0;
    char prompt[PATH_MAX * 2];

    cmd_help(0, 0);
    for (;; g_cur_cmd_hist_pos = (g_cur_cmd_hist_pos + 1) % HISTORY_COUNT) {
        sprintf(prompt, "%s%s$ ", PROMPT, g_cwd);
        read_str = readline(prompt);
        if (!read_str) {
            continue;
        }

        strncpy(g_typed_cmds[g_cur_cmd_hist_pos], read_str, CMDBUF_SIZE);
        run_cmd(); /* cmd string passed by g_typed_cmds */
        free(read_str);
        read_str = 0;
    }
}

