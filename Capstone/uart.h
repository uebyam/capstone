#ifndef UART_H
#define UART_H

#include <stdbool.h>

#define UART_SCB_NUM 1
#define UART_SCB SCB1

// pin 10.0 rx, pin 10.1 tx
#define UART_PRT (10)
#define UART_RX (0)
#define UART_TX (1)

char init_uart(void);

extern bool global_uart_enabled;
extern bool global_uart_host;

#endif
