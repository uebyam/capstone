#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#include "cybsp.h"
#include "cyhal.h"
#include "cy_pdl.h"
#include "cy_retarget_io.h"

#include <ctype.h>
#include <stdlib.h>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

int main() {
    cybsp_init();
    __enable_irq();

    cy_retarget_io_init(P5_1, P5_0, 500000);
    Cy_SCB_UART_Init(comm_scb_HW, &comm_scb_config, 0);
    Cy_SCB_UART_Enable(comm_scb_HW);

    uint16_t imageWidth;
    uint16_t imageHeight;

    uint8_t byte;


    printf("\033[m");
    printf("\033[2J\033[H");
    fflush(stdout);


back:
    while (1) {
        if (!Cy_SCB_UART_GetNumInRxFifo(comm_scb_HW)) continue;

        byte = Cy_SCB_UART_Get(comm_scb_HW);

        if (byte == 'B') {
            goto next;
        }
    }

next:
    for (int i = 0; i < 6;) {
        if (!Cy_SCB_UART_GetNumInRxFifo(comm_scb_HW)) continue;
        Cy_SCB_UART_ClearRxFifo(comm_scb_HW);
        i++;
    }

    Cy_SCB_UART_GetArrayBlocking(comm_scb_HW, &imageWidth, 2);
    Cy_SCB_UART_GetArrayBlocking(comm_scb_HW, &imageHeight, 2);


    int cw = 0;

    printf("%dx%d\r\n", imageWidth, imageHeight);

    char sbuf[256] = {};
    for (int y = 0; y < imageHeight;) {
        uint8_t nextPixel;
        Cy_SCB_UART_GetArrayBlocking(comm_scb_HW, &nextPixel, 1);

        snprintf(sbuf, 256, "\033[48;5;%um ", nextPixel);
        Cy_SCB_UART_PutString(SCB5, sbuf);

        cw++;
        if (cw >= imageWidth) {
            cw = 0;
            Cy_SCB_UART_PutString(SCB5, "\033[m\r\n");
            y++;
        }
    }

    goto back;
}
