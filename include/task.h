#ifndef TASK_H
#define TASK_H

#include "kconfig.h"
#include "list.h"

#define TASK_READY      0
#define TASK_WAIT_READ  1
#define TASK_WAIT_WRITE 2
#define TASK_WAIT_INTR  3
#define TASK_WAIT_TIME  4
#define TASK_WAIT_CHILD 5

/* Stack struct of user thread, see "Exception entry and return" */
struct user_thread_stack {
	unsigned int r4;
	unsigned int r5;
	unsigned int r6;
	unsigned int r7;
	unsigned int r8;
	unsigned int r9;
	unsigned int r10;
	unsigned int fp;
	unsigned int _lr;	/* Back to system calls or return exception */
	unsigned int _r7;	/* Backup from isr */
	unsigned int r0;
	unsigned int r1;
	unsigned int r2;
	unsigned int r3;
	unsigned int ip;
	unsigned int lr;	/* Back to user thread code */
	unsigned int pc;
	unsigned int xpsr;
	unsigned int stack;
};

/* Task Control Block */
struct task_control_block {
    struct user_thread_stack *stack;
    void *stack_start;
    void *stack_end;
    int pid;
    int status;
    int priority;
    int exit_event;

    struct list list;
};

unsigned int *init_task(unsigned int *stack, void (*start)(), size_t stack_size);
struct task_control_block *task_get(int pid);
int task_set_priority(struct task_control_block *task, int priority);
int task_set_stack_size(struct task_control_block *task, size_t size);

#endif
