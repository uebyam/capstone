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
#include <cyhal.h>

#include "dh.h"
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
    cyhal_trng_t trng;

    while (1) {
        // Advertisement loop
        char slave = 0;
        for (;;) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

            if (Cy_SCB_UART_GetNumInRxFifo(UART_SCB)) {
                uint32_t cmd = Cy_SCB_UART_Get(UART_SCB);
                if (cmd == 0xAA) {
                    // Return acknowledgement
                    if (!Cy_SCB_UART_Put(UART_SCB, 0xAB)) {
                        LOG_WARN("Failed to send ack message!\n");
                    } else {
                        LOG_DEBUG("Sent ack to adv\n");
                        slave = 1;
                        break;
                    }
                } else if (cmd == 0xAB) {
                    LOG_DEBUG("Received ack to adv\n");

                    if (!Cy_SCB_UART_Put(UART_SCB, 0xAC)) {
                        LOG_WARN("Failed to send ack message!\n");
                    } else {
                        LOG_DEBUG("Sent ack to ack\n");
                        slave = 0;
                        break;
                    }
                }
            }

            if (!Cy_SCB_UART_Put(UART_SCB, 0xAA)) {
                LOG_WARN("Failed to send adv message!\n");
            } else {
                LOG_DEBUG("Sent adv message\n");
            }
        }

        xTimerChangePeriod(uart_timer_handle, pdMS_TO_TICKS(5), 0);
        xTimerReset(uart_timer_handle, 0);
        ulTaskNotifyTake(pdTRUE, 1);
        if (slave) {
            // Discard ack adv byte
            char fail = 1;
            for (int i = 0; i < 20; i++) {
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                if (Cy_SCB_UART_Get(UART_SCB) == 0xAC) {
                    fail = 0;
                    break;
                }
            }

            // If no ack byte sent, maybe the PSoC who sent the ack adv is dead
            // Reset connection state
            if (fail) {
                xTimerChangePeriod(uart_timer_handle, pdMS_TO_TICKS(100), 0);
                xTimerReset(uart_timer_handle, 0);
                ulTaskNotifyTake(pdTRUE, 1);
                continue;
            }
        }

        // Negotiation loop
        char failures = 0;
        cyhal_trng_init(&trng);
        while (failures < 5) {
            uint32_t randval = cyhal_trng_generate(&trng);
            LOG_DEBUG("Random value: %lu %08lx\n", randval, randval);
            Cy_SCB_UART_PutArray(UART_SCB, &randval, 4);

            int rcv_bytes = 0;
            uint32_t rcvval = 0;
            for (int i = 0; i < 10; i++) {
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                rcv_bytes += Cy_SCB_UART_GetArray(UART_SCB, ((uint8_t*)&rcvval) + rcv_bytes, 4 - rcv_bytes);
                LOG_DEBUG("Received %d bytes so far\n", rcv_bytes);
                if (rcv_bytes == 4) {
                    break;
                }
            }
            if (rcv_bytes != 4) {
                LOG_WARN("Auth failure, didn't receive other number in time\n");
                failures++;
                continue;
            }

            LOG_DEBUG("Their random value: %lu %08lx\n", rcvval, rcvval);

            if (rcvval == randval) {
                LOG_WARN("Auth failure, received value same as local value\n");
                failures++;
                continue;
            }
            break;
        }

        // restart
        if (failures == 5) {
            cyhal_trng_free(&trng);
            xTimerChangePeriod(uart_timer_handle, pdMS_TO_TICKS(100), 0);
            xTimerReset(uart_timer_handle, 0);
            ulTaskNotifyTake(pdTRUE, 1);
            LOG_WARN("Resetting UART connection state; too many failed auth attempts\n");
            continue;
        }


        // otherwise, TRNG disambiguation success
        LOG_INFO("Now doing DH\n");
        ulTaskNotifyTake(pdTRUE, 1);
        for (;;) {
            CY_ALIGN(4) uint16_t secret[8];

            // Generate secret
            {
                uint32_t *p32_secret = (uint32_t*) secret;
                for (int i = 0; i < 4; i++) {
                    p32_secret[i] = cyhal_trng_generate(&trng);
                }
            }

            LOG_DEBUG("Our secret:");
            for (int i = 0; i < 8; i++) {
                LOG_DEBUG_NOFMT(" %u\n", secret[i]);
            } LOG_DEBUG_NOFMT("\n");

            // Send
            uint16_t pub[8] = {};
            dh_compute_public_key(pub, secret, 8, DH_DEFAULT_MOD, DH_DEFAULT_GENERATOR);
            Cy_SCB_UART_PutArray(UART_SCB, pub, 16);

            LOG_DEBUG("Our public:");
            for (int i = 0; i < 8; i++) {
                LOG_DEBUG_NOFMT(" %u\n", pub[i]);
            } LOG_DEBUG_NOFMT("\n");

            // Receive pub
            int has_rcv = 0;
            while (has_rcv < 16) {
                has_rcv += Cy_SCB_UART_GetArray(UART_SCB, ((uint8_t*)pub) + has_rcv, 16 - has_rcv);
                if (has_rcv >= 16) break;
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            }

            LOG_DEBUG("Their public:");
            for (int i = 0; i < 8; i++) {
                LOG_DEBUG_NOFMT(" %u\n", pub[i]);
            } LOG_DEBUG_NOFMT("\n");

            // Compute shared key
            uint16_t key[8] = {};
            dh_compute_shared_secret(key, secret, pub, 8, DH_DEFAULT_MOD);

            LOG_DEBUG("Shared key:");
            for (int i = 0; i < 8; i++) {
                LOG_DEBUG_NOFMT(" %u\n", key[i]);
            } LOG_DEBUG_NOFMT("\n");

            while (1) ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
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
