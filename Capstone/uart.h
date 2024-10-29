#ifndef UART_H
#define UART_H


#define UART_SCB (SCB1)

// pin 10.0 rx, pin 10.1 tx
#define UART_PRT (10)
#define UART_RX (0)
#define UART_TX (1)

void init_uart(void);

#endif
