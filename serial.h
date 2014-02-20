#ifndef SERIAL_H_20140220
#define SERIAL_H_20140220

void rs232_xmit_msg_task();
void serialin(USART_TypeDef* uart, unsigned int intr);
void serialout(USART_TypeDef* uart, unsigned int intr);

#endif
