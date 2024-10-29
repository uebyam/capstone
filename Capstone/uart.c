#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif
#include "uart.h"
#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>
#include <queue.h>

#include "main.h"
#include "ansi.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#define GPIO_POINTER_FOR(port) GPIO_PRT ## port

const cy_stc_scb_uart_config_t uart_cfg = {
	.uartMode = CY_SCB_UART_STANDARD,
	.oversample = 12,		// NOTE: Need to change if core clocks change
	.dataWidth = 8,
	.parity = CY_SCB_UART_PARITY_NONE,
	.stopBits = CY_SCB_UART_STOP_BITS_1,
	.breakWidth = 11,
	.enableMsbFirst = 0
};

void init_uart() {
	Cy_SCB_UART_Init(UART_SCB, &uart_cfg, 0);
	Cy_GPIO_Pin_FastInit(&GPIO->PRT[UART_PRT], UART_RX, CY_GPIO_DM_HIGHZ, 0, 18);
	Cy_GPIO_Pin_FastInit(&GPIO->PRT[UART_PRT], UART_TX, CY_GPIO_DM_STRONG_IN_OFF, 0, 18);
}
