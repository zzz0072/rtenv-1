#include <stddef.h>
#include "stm32f10x.h"
#include "stm32_p103.h"
#include "RTOSConfig.h"

/* rtenv related */
#include "syscall.h"
#include "rt_string.h"
#include "task.h"
#include "file.h"
#include "serial.h"
#include "shell.h"
#include "malloc.h"
#include "event-monitor.h"
#include "path.h"
#include "romdev.h"
#include "romfs.h"
#include "memory-pool.h"


#ifdef UNIT_TEST
#include "unit_test.h"
#endif

/* Global variables */
extern size_t g_task_count;
extern struct task_control_block g_tasks[TASK_LIMIT];
extern struct list ready_list[PRIORITY_LIMIT + 1];  /* [0 ... 39] */
extern unsigned int g_stacks[TASK_LIMIT][STACK_SIZE];

void first()
{
    setpriority(0, 0);

    if (!fork()) setpriority(0, 0), pathserver();
//    if (!fork()) setpriority(0, 0), romdev_driver();
//    if (!fork()) setpriority(0, 0), romfs_server();
    if (!fork()) setpriority(0, 0), serialout(USART2, USART2_IRQn);
    if (!fork()) setpriority(0, 0), serialin(USART2, USART2_IRQn);
    if (!fork()) rs232_xmit_msg_task();
    if (!fork()) setpriority(0, PRIORITY_DEFAULT - 10), shell_task();

    setpriority(0, PRIORITY_LIMIT);
   // mount("/dev/rom0", "/", ROMFS_TYPE, 0);
    while(1);
}

#define INTR_EVENT(intr) (FILE_LIMIT + (intr) + 15) /* see INTR_LIMIT */
#define INTR_EVENT_REVERSE(event) ((event) - FILE_LIMIT - 15)
#define TIME_EVENT (FILE_LIMIT + INTR_LIMIT)

int intr_release(struct event_monitor *monitor, int event,
                 struct task_control_block *task, void *data)
{
    return 1;
}

int time_release(struct event_monitor *monitor, int event,
                 struct task_control_block *task, void *data)
{
    int *tick_count = data;
    return task->stack->r0 == *tick_count;
}


int main()
{
    char memory_space[MEM_LIMIT];
    struct file *files[FILE_LIMIT];
    struct file_request requests[TASK_LIMIT];
    struct list ready_list[PRIORITY_LIMIT + 1];  /* [0 ... 39] */

    struct memory_pool memory_pool;
    struct event events[EVENT_LIMIT];
    unsigned int stacks[TASK_LIMIT][STACK_SIZE];
    struct event_monitor event_monitor;
    size_t current_task = 0;
    int i;
    struct list *list;
    struct task_control_block *task;
    int timeup;
    unsigned int tick_count = 0;

    SysTick_Config(configCPU_CLOCK_HZ / configTICK_RATE_HZ);

    init_rs232();
    __enable_irq();

    /* Initialize memory pool */
    memory_pool_init(&memory_pool, MEM_LIMIT, memory_space);

    /* Initialize all files */
    for (i = 0; i < FILE_LIMIT; i++)
        files[i] = NULL;

    /* Initialize ready lists */
    for (i = 0; i <= PRIORITY_LIMIT; i++)
        list_init(&ready_list[i]);

    /* Initialise event monitor */
    event_monitor_init(&event_monitor, events, ready_list);

    /* Initialize fifos */
    for (i = 0; i <= PATHSERVER_FD; i++)
        file_mknod(i, -1, files, S_IFIFO, &memory_pool, &event_monitor);

    /* Register IRQ events, see INTR_LIMIT */
    for (i = -15; i < INTR_LIMIT - 15; i++)
        event_monitor_register(&event_monitor, INTR_EVENT(i), intr_release, 0);

    event_monitor_register(&event_monitor, TIME_EVENT, time_release, &tick_count);

    /* Init memory pool for dynamic memory allocation */
    init_mpool();

    /* Initialize first task */
    g_tasks[g_task_count].stack = (void*)init_task(stacks[g_task_count], &first);
    g_tasks[g_task_count].tid = 0;
    g_tasks[g_task_count].priority = PRIORITY_DEFAULT;
    list_init(&g_tasks[g_task_count].list);
    list_push(&ready_list[g_tasks[g_task_count].priority], &g_tasks[g_task_count].list);
    g_task_count++;

    while (1) {
        g_tasks[current_task].stack = activate(g_tasks[current_task].stack);
        g_tasks[current_task].status = TASK_READY;
        timeup = 0;

        switch (g_tasks[current_task].stack->r7) {
        case SYS_CALL_FORK: /* fork */
            if (g_task_count == TASK_LIMIT) {
                /* Cannot create a new task, return error */
                g_tasks[current_task].stack->r0 = -1;
            }
            else {
                /* Compute how much of the stack is used */
                size_t used = stacks[current_task] + STACK_SIZE
                          - (unsigned int*)g_tasks[current_task].stack;
                /* New stack is END - used */
                g_tasks[g_task_count].stack = (void*)(stacks[g_task_count] + STACK_SIZE - used);
                /* Copy only the used part of the stack */
                memcpy(g_tasks[g_task_count].stack, g_tasks[current_task].stack,
                       used * sizeof(unsigned int));
                /* Set PID */
                g_tasks[g_task_count].tid = g_task_count;
                /* Set priority, inherited from forked task */
                g_tasks[g_task_count].priority = g_tasks[current_task].priority;
                /* Set return values in each process */
                g_tasks[current_task].stack->r0 = g_task_count;
                g_tasks[g_task_count].stack->r0 = 0;
                list_init(&g_tasks[g_task_count].list);
                list_push(&ready_list[g_tasks[g_task_count].priority],
                          &g_tasks[g_task_count].list);

                /* There is now one more task */
                g_task_count++;
            }
            break;

        case SYS_CALL_GETTID: /* gettid */
            g_tasks[current_task].stack->r0 = current_task;
            break;

        case SYS_CALL_WRITE: /* write */
            {
                /* Check fd is valid */
                int fd = g_tasks[current_task].stack->r0;
                if (fd < FILE_LIMIT && files[fd]) {
                    /* Prepare file request, store reference in r0 */
                    requests[current_task].task = &g_tasks[current_task];
                    requests[current_task].buf =
                        (void*)g_tasks[current_task].stack->r1;
                    requests[current_task].size = g_tasks[current_task].stack->r2;
                    g_tasks[current_task].stack->r0 =
                        (int)&requests[current_task];

                    /* Write */
                    file_write(files[fd], &requests[current_task],
                               &event_monitor);
                }
                else {
                    g_tasks[current_task].stack->r0 = -1;
                }
            } break;

        case SYS_CALL_READ: /* read */
            {
                /* Check fd is valid */
                int fd = g_tasks[current_task].stack->r0;
                if (fd < FILE_LIMIT && files[fd]) {
                    /* Prepare file request, store reference in r0 */
                    requests[current_task].task = &g_tasks[current_task];
                    requests[current_task].buf =
                        (void*)g_tasks[current_task].stack->r1;
                    requests[current_task].size = g_tasks[current_task].stack->r2;
                    g_tasks[current_task].stack->r0 =
                        (int)&requests[current_task];

                    /* Read */
                    file_read(files[fd], &requests[current_task],
                              &event_monitor);
                }
                else {
                    g_tasks[current_task].stack->r0 = -1;
                }
            } break;

        case SYS_CALL_WAIT_INTR: /* interrupt_wait */
            /* Enable interrupt */
            NVIC_EnableIRQ(g_tasks[current_task].stack->r0);

            /* Block task waiting for interrupt to happen */
            event_monitor_block(&event_monitor,
                                INTR_EVENT(g_tasks[current_task].stack->r0),
                                &g_tasks[current_task]);
            g_tasks[current_task].status = TASK_WAIT_INTR;
            break;

        case SYS_CALL_GETPRIORITY: /* getpriority */
            {
                int who = g_tasks[current_task].stack->r0;
                if (who > 0 && who < (int)g_task_count)
                    g_tasks[current_task].stack->r0 = g_tasks[who].priority;
                else if (who == 0)
                    g_tasks[current_task].stack->r0 = g_tasks[current_task].priority;
                else
                    g_tasks[current_task].stack->r0 = -1;
            } break;

        case SYS_CALL_SETPRIORITY: /* setpriority */
            {
                int who = g_tasks[current_task].stack->r0;
                int value = g_tasks[current_task].stack->r1;
                value = (value < 0) ? 0 : ((value > PRIORITY_LIMIT) ? PRIORITY_LIMIT : value);
                if (who > 0 && who < (int)g_task_count) {
                    g_tasks[who].priority = value;
                    if (g_tasks[who].status == TASK_READY)
                        list_push(&ready_list[value], &g_tasks[who].list);
                }
                else if (who == 0) {
                    g_tasks[current_task].priority = value;
                    list_unshift(&ready_list[value], &g_tasks[current_task].list);
                }
                else {
                    g_tasks[current_task].stack->r0 = -1;
                    break;
                }
                g_tasks[current_task].stack->r0 = 0;
            } break;

        case SYS_CALL_MK_NODE: /* mknod */
            g_tasks[current_task].stack->r0 =
                file_mknod(g_tasks[current_task].stack->r0,
                           g_tasks[current_task].tid,
                           files,
                           g_tasks[current_task].stack->r2,
                           &memory_pool,
                           &event_monitor);
            break;

        case SYS_CALL_SLEEP: /* sleep */
            if (g_tasks[current_task].stack->r0 != 0) {
                g_tasks[current_task].stack->r0 += tick_count;
                g_tasks[current_task].status = TASK_WAIT_TIME;
                event_monitor_block(&event_monitor, TIME_EVENT,
                                    &g_tasks[current_task]);
            }
            break;

        case SYS_CALL_LSEEK: /* lseek */
            {
                /* Check fd is valid */
                int fd = g_tasks[current_task].stack->r0;
                if (fd < FILE_LIMIT && files[fd]) {
                    /* Prepare file request, store reference in r0 */
                    requests[current_task].task = &g_tasks[current_task];
                    requests[current_task].buf = NULL;
                    requests[current_task].size = g_tasks[current_task].stack->r1;
                    requests[current_task].whence = g_tasks[current_task].stack->r2;
                    g_tasks[current_task].stack->r0 =
                        (int)&requests[current_task];

                    /* Read */
                    file_lseek(files[fd], &requests[current_task],
                               &event_monitor);
                }
                else {
                    g_tasks[current_task].stack->r0 = -1;
                }
            } break;

        default: /* Catch all interrupts */
                if ((int)g_tasks[current_task].stack->r7 < 0) {
                unsigned int intr = -g_tasks[current_task].stack->r7 - 16;

                if (intr == SysTick_IRQn) {
                    /* Never disable timer. We need it for pre-emption */
                    timeup = 1;
                    tick_count++;
                    event_monitor_release(&event_monitor, TIME_EVENT);
                }
                else {
                    /* Disable interrupt, interrupt_wait re-enables */
                    NVIC_DisableIRQ(intr);
                }
                event_monitor_release(&event_monitor, INTR_EVENT(intr));
            }
        }

        /* Rearrange ready list and event list */
        event_monitor_serve(&event_monitor);

        /* Check whether to context switch */
        task = &g_tasks[current_task];
        if (timeup && ready_list[task->priority].next == &task->list)
            list_push(&ready_list[task->priority], &g_tasks[current_task].list);

        /* Select next TASK_READY task */
        for (i = 0; list_empty(&ready_list[i]); i++);

        list = ready_list[i].next;
        task = list_entry(list, struct task_control_block, list);
        current_task = task->tid;
    }

#ifdef UNIT_TEST
    unit_test();
#endif


    return 0;
}
