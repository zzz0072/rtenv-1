#include <stddef.h>
#include "stm32f10x.h"
#include "stm32_p103.h"
#include "RTOSConfig.h"

/* rtenv related */
#include "syscall.h"
#include "rt_string.h"
#include "task.h"
#include "path_server.h"
#include "serial.h"
#include "shell.h"
#include "malloc.h"

#ifdef UNIT_TEST
#include "unit_test.h"
#endif

/* Global variables */
extern size_t g_task_count;
extern struct task_control_block g_tasks[TASK_LIMIT];


void first()
{
    setpriority(0, 0);

    if (!fork()) setpriority(0, 0), pathserver();
    if (!fork()) setpriority(0, 0), serialout(USART2, USART2_IRQn);
    if (!fork()) setpriority(0, 0), serialin(USART2, USART2_IRQn);
    if (!fork()) rs232_xmit_msg_task();
    if (!fork()) setpriority(0, PRIORITY_DEFAULT - 10), shell_task();

    setpriority(0, PRIORITY_LIMIT);

    while(1);
}

int main()
{
    unsigned int stacks[TASK_LIMIT][STACK_SIZE];
    struct pipe_ringbuffer pipes[PIPE_LIMIT];
    struct task_control_block *ready_list[PRIORITY_LIMIT + 1];  /* [0 ... 39] */
    struct task_control_block *wait_list = NULL;
    size_t current_task = 0;
    size_t i;
    struct task_control_block *task;
    int timeup;
    unsigned int tick_count = 0;

    SysTick_Config(configCPU_CLOCK_HZ / configTICK_RATE_HZ);

    init_rs232();
    __enable_irq();

    g_tasks[g_task_count].stack = (void*)init_task(stacks[g_task_count], &first);
    g_tasks[g_task_count].tid = 0;
    g_tasks[g_task_count].priority = PRIORITY_DEFAULT;
    g_task_count++;

    /* Initialize all pipes */
    for (i = 0; i < PIPE_LIMIT; i++)
        pipes[i].start = pipes[i].end = 0;

    /* Initialize fifos */
    for (i = 0; i <= PATHSERVER_FD; i++)
        _mknod(&pipes[i], S_IFIFO);

    /* Initialize ready lists */
    for (i = 0; i <= PRIORITY_LIMIT; i++)
        ready_list[i] = NULL;

    /* Init memory pool for dynamic memory allocation */
    init_mpool();

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
                g_tasks[g_task_count].prev = NULL;
                g_tasks[g_task_count].next = NULL;
                task_push(&ready_list[g_tasks[g_task_count].priority], &g_tasks[g_task_count]);
                /* There is now one more task */
                g_task_count++;
            }
            break;
        case SYS_CALL_GETTID: /* gettid */
            g_tasks[current_task].stack->r0 = current_task;
            break;
        case SYS_CALL_WRITE: /* write */
            _write(&g_tasks[current_task], g_tasks, g_task_count, pipes);
            break;
        case SYS_CALL_READ: /* read */
            _read(&g_tasks[current_task], g_tasks, g_task_count, pipes);
            break;
        case SYS_CALL_WAIT_INTR: /* interrupt_wait */
            /* Enable interrupt */
            NVIC_EnableIRQ(g_tasks[current_task].stack->r0);
            /* Block task waiting for interrupt to happen */
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
                if (who > 0 && who < (int)g_task_count)
                    g_tasks[who].priority = value;
                else if (who == 0)
                    g_tasks[current_task].priority = value;
                else {
                    g_tasks[current_task].stack->r0 = -1;
                    break;
                }
                g_tasks[current_task].stack->r0 = 0;
            } break;
        case SYS_CALL_MK_NODE: /* mknod */
            if (g_tasks[current_task].stack->r0 < PIPE_LIMIT)
                g_tasks[current_task].stack->r0 =
                    _mknod(&pipes[g_tasks[current_task].stack->r0],
                           g_tasks[current_task].stack->r2);
            else
                g_tasks[current_task].stack->r0 = -1;
            break;
        case SYS_CALL_SLEEP: /* sleep */
            if (g_tasks[current_task].stack->r0 != 0) {
                g_tasks[current_task].stack->r0 += tick_count;
                g_tasks[current_task].status = TASK_WAIT_TIME;
            }
            break;
        default: /* Catch all interrupts */
            if ((int)g_tasks[current_task].stack->r7 < 0) {
                int intr = -g_tasks[current_task].stack->r7 - 16;

                if (intr == SysTick_IRQn) {
                    /* Never disable timer. We need it for pre-emption */
                    timeup = 1;
                    tick_count++;
                }
                else {
                    /* Disable interrupt, interrupt_wait re-enables */
                    NVIC_DisableIRQ(intr);
                }
                /* Unblock any waiting tasks */
                for (i = 0; i < g_task_count; i++)
                    if ((g_tasks[i].status == TASK_WAIT_INTR && g_tasks[i].stack->r0 == intr) ||
                        (g_tasks[i].status == TASK_WAIT_TIME && g_tasks[i].stack->r0 == tick_count))
                        g_tasks[i].status = TASK_READY;
            }
        }

        /* Put waken tasks in ready list */
        for (task = wait_list; task != NULL;) {
            struct task_control_block *next = task->next;
            if (task->status == TASK_READY)
                task_push(&ready_list[task->priority], task);
            task = next;
        }
        /* Select next TASK_READY task */
        for (i = 0; i < (size_t)g_tasks[current_task].priority && ready_list[i] == NULL; i++);
        if (g_tasks[current_task].status == TASK_READY) {
            if (!timeup && i == (size_t)g_tasks[current_task].priority)
                /* Current task has highest priority and remains execution time */
                continue;
            else
                task_push(&ready_list[g_tasks[current_task].priority], &g_tasks[current_task]);
        }
        else {
            task_push(&wait_list, &g_tasks[current_task]);
        }
        while (ready_list[i] == NULL)
            i++;
        current_task = task_pop(&ready_list[i])->tid;
    }

#ifdef UNIT_TEST
    unit_test();
#endif


    return 0;
}
