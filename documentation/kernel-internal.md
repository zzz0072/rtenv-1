# Introduction
This document describes kernel internals.

# Overview
Kernel is the core of rtenv OS. It does:

* Resource pre-allocation, includes
    * ring buffer
    * task control block for ready tasks in different priorities
* Task scheduler
* System calls for task to use
* Hardware initialization
* Bring up tasks

# System call
System calls provides services interface from kernel to user tasks. It's interface implementation is in syscall.s. It merely calls svc (See: `Supervisor Calls`) to switch from thread mode to handler mode (See: `Operating modes`). The difference between implementations are system call number.

* Flow
    * User task calls a system call in thread mode. Functions parameters and return value storage are followed by procedure call standard (See: `Procedure Call Standard for the ARM Architecture`).
    * syscall.s pass system call number in r7 and switch to handler mode.
    * kernel resumed right after activate() and run related system call according to system call number in r7.
    * kernel saves result and put task information into wait queue.
    * Tasks resume eventually and gets the result.

# Scheduler
Here is the sequence of main loop:

* kernel picks a user task and switches to it by setting CONTROL register to thread mode.
* When a system call is invoked in the user task or an interrupt occurred, kernel resumes.
* kernel runs system call or handle interrupt.
    * timer interrupt will run periodically to force context switch and implements sleep system call.
* kernel pushes current task to read queue or waiting queue (only if sleep or wait interrupt).
* kernel decides to run next user task by priority order from waiting queue or to run current task.


# References:
* path_server_internals.md
* [`Operating modes`](http://infocenter.arm.com/help/topic/com.arm.doc.ddi0337e/ch02s01s01.html)
* [`Procedure Call Standard for the ARM Architecture`](http://infocenter.arm.com/help/topic/com.arm.doc.ihi0042e/index.html)
* [`Supervisor Calls`](http://infocenter.arm.com/help/topic/com.arm.doc.dai0179b/ar01s02s07.html)
