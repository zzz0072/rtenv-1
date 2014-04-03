#include <stddef.h>
#include "kconfig.h"
#include "task.h"

/* Global Variables */
size_t g_task_count = 0;
struct task_control_block g_tasks[TASK_LIMIT];

int errno = 0; /* for readdir  */
