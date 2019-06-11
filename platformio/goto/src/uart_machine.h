#ifndef _UARTMACHINEH
#define _UARTMACHINEH

#include "types.h"

#define SERIAL1_SPEED 115200
#define SERIAL1_TIMEOUT 18

void init_uart_machine();
bool read_uart_message();
void reset_uart_buffer();

#endif