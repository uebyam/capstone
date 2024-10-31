#ifndef UART_H
#define UART_H

#include <stdbool.h>
#include <inttypes.h>


#define UART_SCB_NUM 1
#define UART_SCB SCB1

// pin 10.0 rx, pin 10.1 tx
#define UART_PRT (10)
#define UART_RX (0)
#define UART_TX (1)

char init_uart(void);

extern bool global_uart_enabled;

enum {
    UART_MSG_ADV = 0xAA,    // Client broadcasts ADV periodically. Upon receiving an ADV, send back ACK.
    UART_MSG_ACK = 0xAB,    // Client sends ACK immediately followed by 4 random bytes for RNG check
                            // No messages for key exchange or key testing.
    UART_MSG_APP = 0xCA,    // 16-byte encrypted data
    UART_MSG_RESET = 0xA0,  // Reset everything! Go back to advertising.

    // normal domain messages
    UART_MSG_CONN_UNKNOWN       = 0,
    UART_MSG_CONN_KEEPALIVE     = 1,
    UART_MSG_CONN_VOLTMETER     = 2,
};
typedef uint8_t uart_msg_type_t;

#endif
