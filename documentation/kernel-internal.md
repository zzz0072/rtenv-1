# Introduction
This document describes kernel internals.

# Overview
Kernel is the core of rtenv OS. It does:

* Resource pre-allocation, includes
    * ring buffer
    * task control block for ready tasks in different priorities
* Task scheduler
* System calls for tasks to use
* Hardware initialization
* Bring up tasks

# System call
System call provides services interface from kernel to user tasks. It's interface implementation is in syscall.c. It merely calls svc (See: `Supervisor Calls`) to switch from thread mode to handler mode (See: `Operating modes`). The difference between implementations are system call number.

* Flow
    * User task calls a system call in thread mode. Functions parameters and return value storage are followed by procedure call standard (See: `Procedure Call Standard for the ARM Architecture`).
    * syscall.c calls SVC (See: `Supervisor Calls`) and pass system call number in r7, pushes related register include return address in SVC_Handler (See: `ISR`) and switch to handler mode.
    * kernel resumed right after activate() and run related system call according to system call number in r7.
    * kernel saves result and put task information into wait queue.
    * Tasks resume eventually and gets the result.

# Scheduler
Here is the sequence of main loop:

* kernel switches to a user task.
    * Done by
        * Writes r0 (usually system result, See: `Procedure Call Standard for the ARM Architecture`) to process stack pointer
        * Sets CONTROL register options
            * Use process stack pointer
            * Use unprivileged level
        * CPU jumps to return address which stored at SVC_Handler (See: `ISR`) 
* When a system call is invoked in a user task or an interrupt occurred, kernel resumes.
* kernel runs system call or handle interrupt.
    * timer interrupt will run periodically to force context switch and implements sleep system call.
* kernel pushes current task to read queue or waiting queue (only if sleep or wait interrupt).
* kernel decides to run next user task by priority order from waiting queue or decides to run current task.

# Interrupt service routine (`ISR`)
3 ISRs are implemented in rtenv. They are

* SysTick_Handler
* USART2_IRQHandler
* SVC_Handler

Their entry point is declared in `g_pfnVectors` in startup_stm32f10x_md.s. Each time an exception occurred, CPU looks into this table and decide where it will jump to (See #194 in startup_stm32f10x_md for further information of default handler and weak alias.).

The main difference of `SysTick_Handler USART2_IRQHandler` and `SVC_Handler` is that `SysTick_Handler USART2_IRQHandler` are interrupts while `SVC_Handler` is an exception. Thus, the handler of `SysTick_Handler USART2_IRQHandler` have to pass exception via r7 while while `SVC_Handler` does not 

## Interrupt number (See `Interrupt Program Status Register` and `Vector table`)
`Interrupt Program Status Register` (IPSR)'s exception number are IRQ number is different. Thus, rtenv subtracts exception number with 16 to get the IRQ number. Here is the sequence.

* Interrupt occurs
* CPU saves exception number to IPSR, look into vector table, and jump related handler or default handler.
* rtenv registered ISR is called
* rtenv registered ISR saves exception number to r7
* rtenv registered ISR multiple r7 by -1 to let main loop identify the request is an exception or a interrupt
* rtenv kernel resumed
* rtenv kernel convert exception number to IRQ number and did related actions
* rtenv kernel scheduler takes place

# References:
* path_server_internals.md
* [`Operating modes`](http://infocenter.arm.com/help/topic/com.arm.doc.ddi0337e/ch02s01s01.html)
* [`Procedure Call Standard for the ARM Architecture`](http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ihi0042e/index.html)
* [`Supervisor Calls`](http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dai0179b/ar01s02s07.html)
* [`Interrupt Program Status Register`](http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0552a/CHDBIBGJ.html)
* [`Vector table`](http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0552a/BABIFJFG.html)
