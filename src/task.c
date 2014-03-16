#include <task.h>
#include <rt_string.h>
#include <syscall.h>

unsigned int *init_task(unsigned int *stack, void (*start)())
{
    stack += STACK_SIZE - 9; /* End of stack, minus what we're about to push */
    stack[8] = (unsigned int)start;
    return stack;
}


