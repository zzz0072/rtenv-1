# Introduction
This document describes path server internals.

# Backgrounds
## File types
There are 2 types to communicate between 2 tasks.
Both types uses a ringbuffer to store data.

* FIFO
    * Communicated with raw data
    * Created by mkfifo(filename, mode)
        * Actually mode is not used for now
    * Current FIFOs
        * `/tty0/in`
        * `/tty0/out`
* Message queue
    * Send 2 information at a time
        * Buffer size
        * Payload
    * Created by mq_open(filename, O_CREAT)
    * Current message queue
        * `/tmp/mqueue/out`

# How to use?
* FIFO
    * mkfifo(`filename`....);
    * `fd` = open(`filename` ....);
    * read(`fd`, ...);
    * write(`fd`...);
* Message queue
    * `fd` = mq_open(`filename`, O_CREAT); /* Create file */
    * `fd` = mq_open(`filename`, 0);       /* Open file */
    * read(`fd`, ...);
    * write(`fd`...);

# Internals
## kernel
* Kernel creats ringbuffers for each file up to 16. Each file has its unique file descriptor and coresponding ringbuffer.
* Related system calls
    * mknod
        * Modify specific file descriptor's ringbuffer callback function
            * Can be FIFO or message queue
    * read
        * Call FIFO or message queue's callback function  Mainly pop data from ringbuffer and copy to specific addess in read parameter
    * write
        * Call FIFO or message queue's callback function  Mainly write data from specific addess in write parameter to ringbuffer

## Path server
It is a task does
* mknod
* open files by matching file name into its file descriptor
