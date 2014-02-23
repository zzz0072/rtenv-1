# Introduction
This document describes how characters are read from and written to console.

# Overview
To read or write characters in rtenv, the following tasks are involved.

* pathserver
    * Emulate a file system and implements IPC (FIFO)
* serialout
    * read characters from FIFO and write them to USART (console)
        * FIFO: `/tty0/out`
* serialin
    * read input from USART (console) and write to FIFO
        * FIFO: `/tty0/in`
* rs232_xmit_msg_task
    * read characters from queue and write to FIFO
        * queue: `/tmp/mqueue/out`
        * FIFO:  `/tty0/out`

* shell (not limited):
    * read characters from FIFO and write character to queue
        * queue: `/tmp/mqueue/out`
        * FIFO:  `/tty0/in`

# Exection flow:
* Prepare
    1. Creat FIFOs in path_server task
    2. Start serialout and serialin task 

* Display characters on console
    1. Invoke printf(...);
    2. printf write string to `/tmp/mqueue/out`
    3. serialout task read those string from FIFO and write to USART

* Read keys
    1. serialin task reads key from USART and write to `/tty0/in`
    2. shell reads key from `/tty0/in`


# Reference: path_server.md
