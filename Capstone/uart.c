#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#include "uart.h"
bool global_uart_enabled = 0;
bool global_uart_host = 0;

#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>
#include <queue.h>

#include "main.h"
#include "ansi.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#define CASE_RETURN_STR(const)          case const: return #const;

const char *get_scb_uart_status_name(cy_en_scb_uart_status_t status);
const char *get_sysclk_status_name(cy_en_sysclk_status_t status);

TaskHandle_t uart_task_handle;
// NOTE: FreeRTOS imers are susceptible to processor lag.
// TODO: update to use better things than freertos timers
TimerHandle_t uart_timer_handle;

const cy_stc_scb_uart_config_t uart_cfg = {
    .uartMode = CY_SCB_UART_STANDARD,
    .oversample = 12,        // NOTE: Need to change if peripheral clock changes
    .dataWidth = 8,
    .parity = CY_SCB_UART_PARITY_NONE,
    .stopBits = CY_SCB_UART_STOP_BITS_1,
    .breakWidth = 11,
    .enableMsbFirst = 0
};

const cy_stc_sysint_t uart_intr_cfg = {
    .intrSrc = scb_1_interrupt_IRQn,
    .intrPriority = 7
};

uint8_t div_num = 255, div_type;

void uart_timer_task(TimerHandle_t timer) {
    xTaskNotifyGive(uart_task_handle);
}

void uart_task(void *arg) {
    // TODO: diffie-hellman + aes
    char buf;
    char lonely_days = 0;
    char prev_global_uart_host = global_uart_host;
    char uart_started = 0;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (!uart_started && prev_global_uart_host == 0 && global_uart_host == 1) {
            LOG_INFO("Starting UART keepalive as host\n");
        } else prev_global_uart_host = global_uart_host;

        buf = global_uart_host;
        Cy_SCB_UART_PutArray(SCB1, &buf, 1);

        if (Cy_SCB_UART_GetRxFifoStatus(SCB1) & CY_SCB_UART_RX_NOT_EMPTY) {
            LOG_DEBUG("Received data on FIFO\n");
            Cy_SCB_UART_GetArray(SCB1, &buf, 1);
            if (!uart_started) {
                uart_started = 1;
                global_uart_host = 0;

                LOG_INFO("Received presence from other PSoC; starting UART keepalive as \"slave\"\n");
            }
            if (buf == !global_uart_host) {  // Adjust based on expected response
                lonely_days = 0;
                Cy_GPIO_Set(GPIO_PRT13, 7);
                LOG_DEBUG("chocolate cookies\n"); // im a fatty
            } else {
                lonely_days += 1;
                LOG_DEBUG("Lonely day\n");
            }
        } else {
            lonely_days += 1;
        }


        if (lonely_days >= 2) {
            LOG_WARN("More than 2 timer operations have passed! Is other PSoC slacking?\n");
        }
    }
}

char init_uart() {
    cy_en_scb_uart_status_t uart_status = Cy_SCB_UART_Init(UART_SCB, &uart_cfg, 0);
    if (uart_status != CY_SCB_UART_SUCCESS) {
        LOG_ERR("UART block initialisation failed with %s (0x%08x)\n", get_scb_uart_status_name(uart_status), uart_status);
        return 1;
    }
    Cy_GPIO_Pin_FastInit(&GPIO->PRT[UART_PRT], UART_RX, CY_GPIO_DM_HIGHZ, 0, 18);
    Cy_GPIO_Pin_FastInit(&GPIO->PRT[UART_PRT], UART_TX, CY_GPIO_DM_STRONG_IN_OFF, 0, 18);
    
    // we dont want to accidentally use already used peripheral dividers
    div_type = CY_SYSCLK_DIV_8_BIT;
    for (int i = PERI_DIV_8_NR - 1; i >= 0; i--) {
        if (Cy_SysClk_PeriphGetDividerEnabled(div_type, i)) continue;

        div_num = i;
    }

    if (div_num == 255) {
        // couldn't find 8-bit dividers, switch to 16-bit
        LOG_WARN("Couldn't find any free 8-bit periclk dividers; searching for 16-bit dividers\n");
        div_type = CY_SYSCLK_DIV_16_BIT;
        for (int i = PERI_DIV_16_NR - 1; i >= 0; i--) {
            if (Cy_SysClk_PeriphGetDividerEnabled(div_type, i)) continue;

            div_num = i;
        }
        
        if (div_num == 255) {
            LOG_ERR("Couldn't find any free dividers\n");
            return 2;
        }
    }

    // NOTE: need to change if peripheral clock changes
    cy_en_sysclk_status_t sysclk_status = Cy_SysClk_PeriphSetDivider(div_type, div_num, 71);
    if (sysclk_status != CY_SYSCLK_SUCCESS) {
        LOG_ERR("%s Peripheral divider %d initialisation failed with %s (%08x)\n", (div_type == CY_SYSCLK_DIV_8_BIT) ? "8-bit" : "16-bit", div_num, get_sysclk_status_name(sysclk_status), sysclk_status);
        return 2;
    }

    sysclk_status = Cy_SysClk_PeriphEnableDivider(div_type, div_num);
    if (sysclk_status != CY_SYSCLK_SUCCESS) {
        LOG_ERR("Enabling %s peripheral divider %d failed with %s (%08x)\n", (div_type == CY_SYSCLK_DIV_8_BIT) ? "8-bit" : "16-bit", div_num, get_sysclk_status_name(sysclk_status), sysclk_status);
        return 2;
    }

    sysclk_status = Cy_SysClk_PeriphAssignDivider(PCLK_SCB0_CLOCK + UART_SCB_NUM, div_type, div_num);
    if (sysclk_status != CY_SYSCLK_SUCCESS) {
        LOG_ERR("Assigning %s peripheral divider %d failed with %s (%08x)\n", (div_type == CY_SYSCLK_DIV_8_BIT) ? "8-bit" : "16-bit", div_num, get_sysclk_status_name(sysclk_status), sysclk_status);
        return 2;
    }

    LOG_DEBUG("Successfully initialised clock div %d for UART\n", div_type);

    uart_timer_handle = xTimerCreate("UART tx timer", pdMS_TO_TICKS(100), pdTRUE, 0, uart_timer_task);
    if (!uart_timer_handle) {
        LOG_ERR("Failed to create UART tx timer\n");
        return 3;
    } else {
        LOG_DEBUG("Successfully created UART tx timer\n");
    }

    BaseType_t rtos_status = xTaskCreate(uart_task, "UART tx task", configMINIMAL_STACK_SIZE * 4, 0, configMAX_PRIORITIES - 3, &uart_task_handle);
    if (rtos_status != pdPASS) {
        LOG_ERR("Failed to create UART tx task\n");
        return 3;
    } else {
        LOG_DEBUG("Successfully created UART tx task\n");
    }

    Cy_SCB_UART_Enable(UART_SCB);

    xTimerStart(uart_timer_handle, 0);

    return 0;
}

const char *get_scb_uart_status_name(cy_en_scb_uart_status_t status) {
    switch (status) {
        CASE_RETURN_STR(CY_SCB_UART_SUCCESS)
        CASE_RETURN_STR(CY_SCB_UART_BAD_PARAM)
        CASE_RETURN_STR(CY_SCB_UART_RECEIVE_BUSY)
        CASE_RETURN_STR(CY_SCB_UART_TRANSMIT_BUSY)
    }
    return "UNKNOWN_STATUS";
}

const char *get_sysclk_status_name(cy_en_sysclk_status_t status) {
    switch (status) {
        CASE_RETURN_STR(CY_SYSCLK_SUCCESS)
        CASE_RETURN_STR(CY_SYSCLK_BAD_PARAM)
        CASE_RETURN_STR(CY_SYSCLK_TIMEOUT)
        CASE_RETURN_STR(CY_SYSCLK_INVALID_STATE)
        CASE_RETURN_STR(CY_SYSCLK_UNSUPPORTED_STATE)
    }

    return "UNKNOWN_STATUS";
}
