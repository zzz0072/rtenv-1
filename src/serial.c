#include "syscall.h"
#include "kconfig.h"
#include "fifo.h"
#include "mqueue.h"
#include "stm32f10x.h"
void serialout(USART_TypeDef* uart, unsigned int intr)
{
    int fd;
    char c;
    int doread = 1;
    mkfifo("/dev/tty0/out", 0);
    fd = open("/dev/tty0/out", 0);

    while (1) {
        if (doread)
            read(fd, &c, 1);
        doread = 0;
        if (USART_GetFlagStatus(uart, USART_FLAG_TXE) == SET) {
            USART_SendData(uart, c);
            USART_ITConfig(USART2, USART_IT_TXE, ENABLE);
            doread = 1;
        }
        interrupt_wait(intr);
        USART_ITConfig(USART2, USART_IT_TXE, DISABLE);
    }
}

void serialin(USART_TypeDef* uart, unsigned int intr)
{
    int fd;
    char c;
    mkfifo("/dev/tty0/in", 0);
    fd = open("/dev/tty0/in", 0);

    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    while (1) {
        interrupt_wait(intr);
        if (USART_GetFlagStatus(uart, USART_FLAG_RXNE) == SET) {
            c = USART_ReceiveData(uart);
            write(fd, &c, 1);
        }
    }
}

void rs232_xmit_msg_task()
{
    int fdout;
    int fdin;
    char str[100];
    int curr_char;

    fdout = open("/dev/tty0/out", 0);
    fdin = mq_open("/tmp/mqueue/out", O_CREAT);
    setpriority(0, PRIORITY_DEFAULT - 2);

    while (1) {
        /* Read from the queue.  Keep trying until a message is
         * received.  This will block for a period of time (specified
         * by portMAX_DELAY). */
        read(fdin, str, 100);

        /* Write each character of the message to the RS232 port. */
        curr_char = 0;
        while (str[curr_char] != '\0') {
            write(fdout, &str[curr_char], 1);
            curr_char++;
        }
    }
}


