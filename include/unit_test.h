#ifndef UNIT_TEST_H
#define UNIT_TEST_H

#include "shell.h"

extern char g_typed_cmds[HISTORY_COUNT][CMDBUF_SIZE];
extern int cur_his;

extern evar_entry g_env_var[MAX_ENVCOUNT];
extern int g_env_var_count;

void unit_test();

#endif
