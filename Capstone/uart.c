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
#include "eepromManager.h"

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
    .oversample = 8,        // NOTE: Need to change if peripheral clock changes
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

char uart_next_msg = UART_MSG_CONN_KEEPALIVE;

const uint16_t UART_CONN_SLOW_TICKS = pdMS_TO_TICKS(100);
const uint16_t UART_CONN_ACTIVE_TICKS = pdMS_TO_TICKS(5);
const uint16_t UART_CONN_DESYNC_TICKS = pdMS_TO_TICKS(43);

void handle_uart_msg(uint32_t cmd, uint8_t* buf, uint16_t* errvar);

void uart_task(void *arg) {
    // TODO: diffie-hellman + aes
    cyhal_trng_t trng;

    const uint16_t UART_CONN_FAILURE_THRESHOLD = 10;

    while (1) {
        // Advertisement loop
        bool expecting_AB = false;
        for (;;) {
            if (Cy_SCB_UART_GetNumInRxFifo(UART_SCB)) {
                Cy_SCB_UART_ClearRxFifo(UART_SCB);
            }
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

            if (Cy_SCB_UART_GetNumInRxFifo(UART_SCB)) {
                uint32_t cmd = Cy_SCB_UART_Get(UART_SCB);
                if (cmd == 0xAA || cmd == 0xAB) {
                    if (Cy_SCB_UART_Put(UART_SCB, 0xAB)) {
                        expecting_AB = cmd == 0xAA;
                        break;
                    }
                }
            }

            if (!Cy_SCB_UART_Put(UART_SCB, 0xAA)) {
                LOG_DEBUG("Failed to send adv message\n");
            }
        }

        xTimerChangePeriod(uart_timer_handle, UART_CONN_ACTIVE_TICKS, 0);
        xTimerReset(uart_timer_handle, 0);
        ulTaskNotifyTake(pdTRUE, 1);
        if (expecting_AB) {
            // Discard AB byte
            bool fail = false;
            for (int i = 0; i < 20; i++) {
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                if (Cy_SCB_UART_Get(UART_SCB) == 0xAB) {
                    fail = false;
                    break;
                }
            }

            if (fail) {
                LOG_DEBUG("Failed to receive expected ADV ACK byte; possible feedback?\n");
                Cy_SCB_UART_Put(UART_SCB, UART_MSG_RESET);
                xTimerChangePeriod(uart_timer_handle, UART_CONN_SLOW_TICKS, 0);
                xTimerReset(uart_timer_handle, 0);
                ulTaskNotifyTake(pdTRUE, 1);
                continue;
            }
        }

        LOG_DEBUG("Received UART signals from potential BMS, performing TRNG check\n");

        // TRNG check loop
        char failures = 0;
        cyhal_trng_init(&trng);
        while (failures < 5) {
            uint32_t rand_val = cyhal_trng_generate(&trng);
            Cy_SCB_UART_PutArray(UART_SCB, &rand_val, 4);

            int rcv_bytes = 0;
            uint32_t rcv_val = 0;
            for (int i = 0; i < 10; i++) {
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                rcv_bytes += Cy_SCB_UART_GetArray(UART_SCB, ((uint8_t*)&rcv_val) + rcv_bytes, 4 - rcv_bytes);
                if (rcv_bytes == 4) break;
            }

            if (rcv_bytes != 4) {
                LOG_DEBUG("TRNG check failure, didn't receive other number in time\n");
                failures++;
                continue;
            }

            if (rcv_val == rand_val) {
                LOG_DEBUG("TRNG check failure, received same value as local value\n");
                failures++;
                continue;
            }
            break;
        }

        // restart
        if (failures == 5) {
            cyhal_trng_free(&trng);
            xTimerChangePeriod(uart_timer_handle, UART_CONN_SLOW_TICKS, 0);
            xTimerReset(uart_timer_handle, 0);
            ulTaskNotifyTake(pdTRUE, 1);
            LOG_DEBUG("Resetting UART connection state; too many failed auth attempts\n");
            Cy_SCB_UART_Put(UART_SCB, UART_MSG_RESET);
            continue;
        }


        LOG_DEBUG("Now doing DH\n");
        uint16_t our_id = 0, their_id = 0;
        bool fail = true;
        CY_ALIGN(4) uint16_t their_pub[8];
        CY_ALIGN(4) uint16_t our_pub[8] = {};
        CY_ALIGN(4) uint16_t secret[8];
        CY_ALIGN(4) uint8_t key[16] = {};
        for (int j = 0; j < 5; j++) {
            // CY_ALIGN(4) to prevent misaligned r/w
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

            // Generate secret
            {
                uint32_t *p32_secret = (uint32_t*) secret;
                for (int i = 0; i < 4; i++) {
                    p32_secret[i] = cyhal_trng_generate(&trng);
                }
            }

            // Send
            dh_compute_public_key(our_pub, secret, 8, DH_DEFAULT_MOD, DH_DEFAULT_GENERATOR);
            Cy_SCB_UART_PutArray(UART_SCB, our_pub, 16);

            // Receive pub
            int rcv_bytes = 0;
            for (int i = 0; i < 5; i++) {
                rcv_bytes += Cy_SCB_UART_GetArray(UART_SCB, ((uint8_t*)their_pub) + rcv_bytes, 16 - rcv_bytes);
                if (rcv_bytes >= 16) break;
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            }

            if (rcv_bytes < 16) {
                continue;
            }

            // Compute shared key
            dh_compute_shared_secret((uint16_t*)key, secret, their_pub, 8, DH_DEFAULT_MOD);

            LOG_DEBUG("Shared key:");
            for (int i = 0; i < 8; i++) {
                LOG_DEBUG_NOFMT(" %04x", ((uint16_t*)key)[i]);
            } LOG_DEBUG_NOFMT("\n");

            int i = 0;
            for (; i < 8; i++) {
                if (our_pub[i] != their_pub[i]) {
                    their_id = their_pub[i];
                    our_id = our_pub[i];
                    fail = false;
                    break;
                }
            }

            break;
        }

        if (fail) {
            LOG_DEBUG("DH failed, resetting\n");
            Cy_SCB_UART_Put(UART_SCB, UART_MSG_RESET);
            cyhal_trng_free(&trng);
            xTimerChangePeriod(uart_timer_handle, UART_CONN_SLOW_TICKS, 0);
            xTimerReset(uart_timer_handle, 0);
            ulTaskNotifyTake(pdTRUE, 1);
            continue;
        }

        ulTaskNotifyTake(pdTRUE, 1);

        cyhal_trng_free(&trng);

        if (Cy_Crypto_Core_Enable(CRYPTO) != CY_CRYPTO_SUCCESS) {
            LOG_ERR("Crypto core could not be initialised; halting UART\n");
            xTimerStop(uart_timer_handle, 0);
            for (;;) ulTaskNotifyTake(pdTRUE, 1);
            continue;
        }

        // Encrypted message chain starts with "verification"
        const uint8_t msg_yea[16] = "verification";
        // Reuse buffers
        uint8_t* our_last_block = (uint8_t*)our_pub;
        uint8_t* their_last_block = (uint8_t*)their_pub;
        uint8_t* plainbuf = (uint8_t*)key;
        uint8_t* encbuf = (uint8_t*)secret;
        CY_ALIGN(4) uint8_t ivbuf[16] = {};
        cy_stc_crypto_aes_state_t aes_state;
        Cy_Crypto_Core_Aes_Init(CRYPTO, key, CY_CRYPTO_KEY_AES_128, &aes_state);

        // Same key verification
        {
            uint16_t tmp_id = 0;
            bool failed = 1;
            for (int i = 0; i < 5; i++) {
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY); 

                // Send an encrypted block
                memcpy(plainbuf, msg_yea, 16);
                Cy_Crypto_Core_Aes_Ecb(CRYPTO, CY_CRYPTO_ENCRYPT, encbuf, plainbuf, &aes_state);

                LOG_DEBUG("Sending our ID %04x\n", our_id);
                Cy_SCB_UART_PutArray(UART_SCB, &our_id, 2);
                Cy_SCB_UART_PutArray(UART_SCB, encbuf, 16);

                ulTaskNotifyTake(pdTRUE, portMAX_DELAY); 

                // Receive encrypted block into `encbuf'
                int rcv_bytes = 0;
                for (int j = 0; j < 20; j++) {
                    rcv_bytes += Cy_SCB_UART_GetArray(UART_SCB, ((uint8_t*)&tmp_id) + rcv_bytes, 2 - rcv_bytes);
                    if (rcv_bytes >= 2) break;
                    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                }
                xTimerChangePeriod(uart_timer_handle, UART_CONN_ACTIVE_TICKS, 0);
                xTimerReset(uart_timer_handle, 0);
                ulTaskNotifyTake(pdTRUE, 1);
                if (rcv_bytes < 2) {
                    LOG_DEBUG("ID error (did not receive bytes, only received %d), resetting\n", rcv_bytes);
                    break;
                }
                if (tmp_id != their_id || tmp_id == our_id) {
                    LOG_DEBUG("ID error, resetting\n");
                    break;
                }
                rcv_bytes = 0;
                for (int j = 0; j < 5; j++) {
                    rcv_bytes += Cy_SCB_UART_GetArray(UART_SCB, &encbuf[rcv_bytes], 16 - rcv_bytes);
                    if (rcv_bytes >= 16) break;
                    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                }

                // Failed
                if (rcv_bytes < 16) continue;

                // Check sent message (now in plainbuf)
                Cy_Crypto_Core_Aes_Ecb(CRYPTO, CY_CRYPTO_DECRYPT, plainbuf, encbuf, &aes_state);
                int j;
                for (j = 0; j < 16; j++) if (plainbuf[j] != msg_yea[j]) break;

                // If loop was broken prematurely, not every byte passed the check.
                if (j != 16) break;
                failed = 0;
                break;
            }

            xTimerChangePeriod(uart_timer_handle, UART_CONN_SLOW_TICKS, 0);
            xTimerReset(uart_timer_handle, 0);
            ulTaskNotifyTake(pdTRUE, 1);

            if (failed) {
                LOG_DEBUG("AES key check failed, check connection; resetting\n");
                Cy_SCB_UART_Put(UART_SCB, UART_MSG_RESET);
                Cy_Crypto_Core_Aes_Free(CRYPTO, &aes_state);
                Cy_Crypto_Core_Disable(CRYPTO);
                Cy_Crypto_Core_ClearVuRegisters(CRYPTO);
                continue;
            }
        }


        // Real loop
        LOG_INFO("\033[1;32mConnected with BMS\033[m\n");

        uint16_t lonely_days = 0;
        for (;;) {
            if (lonely_days > 1) {
                LOG_DEBUG("Possible UART connection failure\n");
            }
            if (lonely_days > UART_CONN_FAILURE_THRESHOLD) {
                LOG_DEBUG("More than %d communication failures; resetting\n", UART_CONN_FAILURE_THRESHOLD);
                Cy_SCB_UART_Put(UART_SCB, UART_MSG_RESET);
                LOG_INFO("\033[;1;97;48;5;196mTamper detected:\033[;1;38;5;196m Disconnected from BMS\033[m\n");
                Cy_Crypto_Core_Aes_Free(CRYPTO, &aes_state);
                Cy_Crypto_Core_Disable(CRYPTO);
                Cy_Crypto_Core_ClearVuRegisters(CRYPTO);
                increaseTamperCount(EEPROM_TAMPER_TYPE_UART_DISCONNECT);
				xTaskNotifyGive(get_ess_handle());
                break;
            }

            int num_in_rx_buf = Cy_SCB_UART_GetNumInRxFifo(UART_SCB);
            if (num_in_rx_buf == 0 && lonely_days < 2) {
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            } else if (num_in_rx_buf < 16) {
                xTimerChangePeriod(uart_timer_handle, UART_CONN_DESYNC_TICKS, 0);
                xTimerReset(uart_timer_handle, 0);
                ulTaskNotifyTake(pdTRUE, 1);

                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

                xTimerChangePeriod(uart_timer_handle, UART_CONN_SLOW_TICKS, 0);
                xTimerReset(uart_timer_handle, 0);
                ulTaskNotifyTake(pdTRUE, 1);
            }

            if (!num_in_rx_buf) {
                char next_msg = uart_next_msg;
                plainbuf[0] = next_msg;

                // last 2 bytes are our id
                ((uint16_t*)plainbuf)[7] = our_id;
                
                memcpy(ivbuf, our_last_block, 16);
                Cy_Crypto_Core_Aes_Cbc(CRYPTO, CY_CRYPTO_ENCRYPT, 16, ivbuf, encbuf, plainbuf, &aes_state);

                memcpy(our_last_block, encbuf, 16);
                Cy_SCB_UART_Put(UART_SCB, UART_MSG_APP);
                Cy_SCB_UART_PutArray(UART_SCB, encbuf, 16);
            }

            // Receive encrypted block into `encbuf'
            int rcv_bytes = 0;
            uint32_t cmd = 0xFFFFFFFF;
            if (Cy_SCB_UART_GetNumInRxFifo(UART_SCB)) {
                xTimerChangePeriod(uart_timer_handle, UART_CONN_ACTIVE_TICKS, 0);
                xTimerReset(uart_timer_handle, 0);
                ulTaskNotifyTake(pdTRUE, 1);
                cmd = Cy_SCB_UART_Get(UART_SCB);

                if (cmd == 0xFFFFFFFF) {
                    LOG_DEBUG("Reported elements in rx buffer, but get failed\n");
                    lonely_days++;
                    continue;
                }

                if (cmd == UART_MSG_APP) {
                    for (int j = 0; j < 5; j++) {
                        rcv_bytes += Cy_SCB_UART_GetArray(UART_SCB, &encbuf[rcv_bytes], 16 - rcv_bytes);
                        if (rcv_bytes >= 16) break;
                        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                    }

                    // Failed
                    if (rcv_bytes < 16) {
                        lonely_days += lonely_days + 1;
                        continue;
                    }


                } else if (cmd == UART_MSG_RESET || cmd == UART_MSG_ADV) {
                    // reset
                    LOG_DEBUG("External reset requested; resetting\n");
                    xTimerChangePeriod(uart_timer_handle, UART_CONN_SLOW_TICKS, 0);
                    xTimerReset(uart_timer_handle, 0);
                    ulTaskNotifyTake(pdTRUE, 1);
                    LOG_INFO("\033[;1;97;48;5;196mTamper detected:\033[;1;38;5;196m Disconnected from BMS\033[m\n");
                    Cy_Crypto_Core_Aes_Free(CRYPTO, &aes_state);
                    Cy_Crypto_Core_Disable(CRYPTO);
                    Cy_Crypto_Core_ClearVuRegisters(CRYPTO);
                    increaseTamperCount(EEPROM_TAMPER_TYPE_UART_DISCONNECT);
					xTaskNotifyGive(get_ess_handle());
                    break;
                } else {
                    LOG_DEBUG("Unknown command %02lx\n", cmd);
                    
                }

                xTimerChangePeriod(uart_timer_handle, UART_CONN_SLOW_TICKS, 0);
                xTimerReset(uart_timer_handle, 0);
                ulTaskNotifyTake(pdTRUE, 1);
            } else {
                // No data; count event and continue
                lonely_days += lonely_days + 1;
                continue;
            }

            // Their message should not be identical to ours!
            int j;
            for (j = 0; j < 16; j++) if (encbuf[j] != our_last_block[j]) break;
            if (j == 16) {
                LOG_DEBUG("Received message same as sent message; possible feedback\n");
                // feedback is banned so it counts as 5 lonely days
                lonely_days += 5;
                continue;
            }

            memcpy(ivbuf, their_last_block, 16);
            memcpy(their_last_block, encbuf, 16);

            // Check received message (now in plainbuf)
            Cy_Crypto_Core_Aes_Cbc(CRYPTO, CY_CRYPTO_DECRYPT, 16, ivbuf, plainbuf, encbuf, &aes_state);
            // Last 2 bytes are ID
            if (((uint16_t*)plainbuf)[7] != their_id) {
                LOG_DEBUG("Last 2 bytes of received message (%u) isn't their id (%u)", ((uint16_t*)plainbuf)[7], their_id);
                lonely_days++;

                if (((uint16_t*)plainbuf)[7] == our_id) {
                    LOG_DEBUG_NOFMT(", it's ours!!!\n");
                    lonely_days += 5;

                    continue;
                } else {
                    LOG_DEBUG_NOFMT("\n");
                }
            }

            handle_uart_msg(cmd, plainbuf, &lonely_days);
        }
    }
}

char init_uart() {
    cy_en_scb_uart_status_t uart_status = Cy_SCB_UART_Init(UART_SCB, &uart_cfg, 0);
    if (uart_status != CY_SCB_UART_SUCCESS) {
        LOG_ERR("UART block initialisation failed with %s (0x%08x)\n", get_scb_uart_status_name(uart_status), uart_status);
        return 1;
    }
    Cy_GPIO_Pin_FastInit(&GPIO->PRT[UART_PRT], UART_RX, CY_GPIO_DM_PULLUP, 0, 18);
    Cy_GPIO_Pin_FastInit(&GPIO->PRT[UART_PRT], UART_TX, CY_GPIO_DM_STRONG_IN_OFF, 0, 18);

    // we dont want to accidentally use already used peripheral dividers
    div_type = CY_SYSCLK_DIV_8_BIT;
    for (int i = PERI_DIV_8_NR - 1; i >= 0; i--) {
        if (Cy_SysClk_PeriphGetDividerEnabled(div_type, i)) continue;

        div_num = i;
    }

    if (div_num == 255) {
        // couldn't find 8-bit dividers, switch to 16-bit
        LOG_DEBUG("Couldn't find any free 8-bit periclk dividers; searching for 16-bit dividers\n");
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
    cy_en_sysclk_status_t sysclk_status = Cy_SysClk_PeriphSetDivider(div_type, div_num, 53);
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

    uart_timer_handle = xTimerCreate("UART tx timer", UART_CONN_SLOW_TICKS, pdTRUE, 0, uart_timer_task);
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



void handle_uart_msg(uint32_t cmd, uint8_t* buf, uint16_t* errvar) {
    if (cmd == 0xFFFFFFFF) return;
    static uint8_t last_msg = UART_MSG_CONN_KEEPALIVE;

    switch (buf[0]) {
        case UART_MSG_CONN_KEEPALIVE:
            // Normal keepalive
            if (last_msg == UART_MSG_CONN_VOLTMETER) {
                LOG_INFO("\033[1;32mBattery voltage restored\033[m\n");
            }

            *errvar = 0;
            break;

        case UART_MSG_CONN_VOLTMETER:
            // Voltmeter reading
            if (last_msg == UART_MSG_CONN_KEEPALIVE) {
                float32_t volts;
                for (int i = 0; i < 4; i++) {
                    ((uint8_t*)&volts)[i] = buf[i + 1];
                }
                LOG_INFO("\033[;1;97;48;5;196mTamper detected:\033[;1;38;5;196m Battery voltage dropped to %fv\033[m\n", volts);
                increaseTamperCount(EEPROM_TAMPER_TYPE_BATTERY_VOLTAGE);
				xTaskNotifyGive(get_ess_handle());
            }
            *errvar = 0;
            break;

        default:
            LOG_DEBUG("Unknown message sent, counting as failure\n");
            (*errvar)++;
            break;
    }

    last_msg = buf[0];
}
