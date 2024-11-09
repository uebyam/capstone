#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#include <cyhal.h>
#include <cybsp.h>
#include <cy_pdl.h>
#include <cy_retarget_io.h>

#include "dh.h"
#include "ansi.h"
#include "uart.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

void init_uart_on_scb1();
const char *get_uart_msg_type_name(uart_msg_type_t name);

bool rangeIntr = false;
int16_t lastCode = 0;

void range_isr() {
    uint32_t intr = Cy_SAR_GetInterruptStatusMasked(SAR);
    Cy_SAR_ClearInterrupt(SAR, intr);
    
    intr = Cy_SAR_GetRangeInterruptStatusMasked(SAR);
    if (intr) {
        Cy_SAR_ClearRangeInterrupt(SAR, intr);
        rangeIntr = true;
        lastCode = Cy_SAR_GetResult16(SAR, 0);
    }
    NVIC_ClearPendingIRQ(pass_0_sar_0_IRQ);
    
}

int main() {
    cybsp_init();
    __enable_irq();

    Cy_SysClk_PllDisable(1);
    Cy_SysClk_FllDisable();
    Cy_SysClk_FllConfigure(8000000, 50000000, CY_SYSCLK_FLLPLL_OUTPUT_OUTPUT);
    Cy_SysClk_FllEnable(500);
    Cy_SysPm_SwitchToSimoBuck();
    SystemCoreClockUpdate();

    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, (uint32_t)115200);

    LOG_INFO("\nApp started\n");

    init_uart_on_scb1();
    Cy_GPIO_Pin_FastInit(GPIO_PRT13, 7, CY_GPIO_DM_STRONG_IN_OFF, 1, P13_7_GPIO);

    init_cycfg_pins();
	init_cycfg_clocks();
	init_cycfg_peripherals();
    Cy_SysAnalog_Init(&pass_0_aref_0_config);
    Cy_SysAnalog_Enable();

    cy_stc_sysint_t sar_intr_cfg = {
        .intrSrc = pass_0_sar_0_IRQ,
        .intrPriority = 7,
    };
    Cy_SysInt_Init(&sar_intr_cfg, range_isr);
    NVIC_EnableIRQ(pass_0_sar_0_IRQ);

    cy_en_sar_status_t status;
    status = Cy_SAR_Init(SAR, &pass_0_sar_0_config);
    if (CY_SAR_SUCCESS == status) {
        Cy_SAR_Enable(SAR);
        Cy_SAR_StartConvert(SAR, CY_SAR_START_CONVERT_CONTINUOUS);
    } // adc

    LOG_INFO("UART initialised, waiting for user button or other PSoC to send message\n");

    cyhal_trng_t trng;

    while (1) {
        char slave = 0;
        Cy_GPIO_Clr(GPIO_PRT13, 7);
        for (;;) {
            // Fix for deadlock loop
            // Suspected cause: uncleared RX buffer from previous connection
            // interferes with advertisement, advertisement loop clears bytes
            // too slow to ever reach bottom of buffer
            if (Cy_SCB_UART_GetNumInRxFifo(SCB1)) {
                Cy_SCB_UART_ClearRxFifo(SCB1);
            }
            Cy_SysLib_Delay(100);

            if (Cy_SCB_UART_GetNumInRxFifo(SCB1)) {
                uint32_t cmd = Cy_SCB_UART_Get(SCB1);
                if (cmd == 0xAA) {
                    // Return acknowledgement
                    if (!Cy_SCB_UART_Put(SCB1, 0xAB)) {
                        LOG_WARN("Failed to send ack message!\n");
                    } else {
                        LOG_DEBUG("Sent ack to adv\n");
                        slave = 1;
                        break;
                    }
                } else if (cmd == 0xAB) {
                    LOG_DEBUG("Received ack to adv\n");

                    if (!Cy_SCB_UART_Put(SCB1, 0xAB)) {
                        LOG_WARN("Failed to send ack message!\n");
                    } else {
                        LOG_DEBUG("Sent ack to ack\n");
                        slave = 0;
                        break;
                    }
                }
            }

            if (!Cy_SCB_UART_Put(SCB1, 0xAA)) {
                LOG_WARN("Failed to send adv message!\n");
            } else {
                LOG_DEBUG("Sent adv message\n");
            }
        }

        if (slave) {
            char fail = 1;
            for (int i = 0; i < 20; i++) {
                Cy_SysLib_Delay(5);
                if (Cy_SCB_UART_Get(SCB1) == 0xAB) {
                    fail = 0;
                    LOG_DEBUG("Received ack ack\n");
                    break;
                }
            }

            if (fail) {
                LOG_WARN("Didn't receive ack ack\n");
                continue;
            }
        }

        // Negotiation loop
        char failures = 0;
        cyhal_trng_init(&trng);
        while (failures < 5) {
            uint32_t randval = cyhal_trng_generate(&trng);
            LOG_DEBUG("Random value: %lu %08lx\n", randval, randval);
            Cy_SCB_UART_PutArray(SCB1, &randval, 4);

            int rcv_bytes = 0;
            uint32_t rcvval = 0;
            for (int i = 0; i < 10; i++) {
                Cy_SysLib_Delay(5);
                rcv_bytes += Cy_SCB_UART_GetArray(SCB1, ((uint8_t*)&rcvval) + rcv_bytes, 4 - rcv_bytes);
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
            LOG_WARN("Resetting UART connection state; too many failed auth attempts\n");
            continue;
        }


        // otherwise, TRNG disambiguation success
        // note: DH process is susceptible to feedback
        LOG_INFO("Now doing DH\n");
        // for future use
        uint16_t our_id = 0, their_id = 0;
        uint16_t key[8] = {};
        uint16_t their_pub[8];
        uint16_t our_pub[8] = {};
        bool fail = true;
        for (int j = 0; j < 5; j++) {
            CY_ALIGN(4) uint16_t secret[8];
            Cy_SysLib_Delay(5);

            // Generate secret
            {
                uint32_t *p32_secret = (uint32_t*) secret;
                for (int i = 0; i < 4; i++) {
                    p32_secret[i] = cyhal_trng_generate(&trng);
                }
            }

            LOG_DEBUG("Our secret:");
            for (int i = 0; i < 8; i++) {
                LOG_DEBUG_NOFMT(" %04x", secret[i]);
            } LOG_DEBUG_NOFMT("\n");

            // Send
            dh_compute_public_key(our_pub, secret, 8, DH_DEFAULT_MOD, DH_DEFAULT_GENERATOR);
            Cy_SCB_UART_PutArray(SCB1, our_pub, 16);

            LOG_DEBUG("Our public:");
            for (int i = 0; i < 8; i++) {
                LOG_DEBUG_NOFMT(" %04x", our_pub[i]);
            } LOG_DEBUG_NOFMT("\n");

            // Receive pub
            int has_rcv = 0;
            for (int i = 0; i < 5; i++) {
                has_rcv += Cy_SCB_UART_GetArray(SCB1, ((uint8_t*)their_pub) + has_rcv, 16 - has_rcv);
                if (has_rcv >= 16) break;
                Cy_SysLib_Delay(5);
            }

            if (has_rcv < 16) {
                LOG_WARN("Received less than 16 bytes for DH, trying again\n");
                continue;
            }

            LOG_DEBUG("Their public:");
            for (int i = 0; i < 8; i++) {
                LOG_DEBUG_NOFMT(" %04x", their_pub[i]);
            } LOG_DEBUG_NOFMT("\n");

            for (int i = 0; i < 8; i++) {
                if (our_pub[i] != their_pub[i]) {
                    their_id = their_pub[i];
                    our_id = our_pub[i];
                    LOG_DEBUG("Index %d\n", i);
                    LOG_DEBUG("Their ID: %u %04x, our ID: %u %04x\n", their_id, their_id, our_id, our_id);
                    break;
                }
            }

            // Compute shared key
            dh_compute_shared_secret(key, secret, their_pub, 8, DH_DEFAULT_MOD);

            LOG_INFO("Shared key:");
            for (int i = 0; i < 8; i++) {
                LOG_DEBUG_NOFMT(" %04x", key[i]);
            } LOG_DEBUG_NOFMT("\n");

            fail = false;
            break;
        }

        if (fail) {
            LOG_ERR("DH failed, resetting\n");
            continue;
        }

        cyhal_trng_free(&trng);

        if (Cy_Crypto_Core_Enable(CRYPTO) != CY_CRYPTO_SUCCESS) {
            LOG_ERR("Crypto core could not be initialised; halting UART\n");
            for (;;) Cy_SysLib_Delay(1000);
            continue;
        } else {
            LOG_DEBUG("Crypto initialised\n");
        }

        // Encrypted message chain starts with "verification"
        const uint8_t msg_yea[16] = "verification";
        uint8_t* our_last_block = (uint8_t*)our_pub;
        uint8_t* their_last_block = (uint8_t*)their_pub;
        uint8_t srcbuf[16] = {};
        uint8_t sendbuf[16] = {};
        uint8_t ivbuf[16] = {};
        cy_stc_crypto_aes_state_t aes_state;
        Cy_Crypto_Core_Aes_Init(CRYPTO, (uint8_t*)key, CY_CRYPTO_KEY_AES_128, &aes_state);

        // Same key verification
        LOG_INFO("Now performing symmetric key verification\n");
        {
            uint16_t tmp_id = 0;
            bool failed = 1;
            for (int i = 0; i < 5; i++) {
                Cy_SysLib_Delay(5);

                // Send an encrypted block
                memcpy(srcbuf, msg_yea, 16);
                Cy_Crypto_Core_Aes_Ecb(CRYPTO, CY_CRYPTO_ENCRYPT, sendbuf, srcbuf, &aes_state);

                LOG_DEBUG("Sending encrypted block:");
                for (int j = 0; j < 16; j++) { LOG_DEBUG_NOFMT(" %02x", sendbuf[j]); }
                LOG_DEBUG_NOFMT("\n");

                Cy_SCB_UART_PutArray(SCB1, &our_id, 2);
                Cy_SCB_UART_PutArray(SCB1, sendbuf, 16);


                Cy_SysLib_Delay(5);

                // Receive encrypted block into `sendbuf'
                int rcv_bytes = 0;
                for (int j = 0; j < 20; j++) {
                    rcv_bytes += Cy_SCB_UART_GetArray(SCB1, ((uint8_t*)&tmp_id) + rcv_bytes, 2 - rcv_bytes);
                    if (rcv_bytes >= 2) break;
                    Cy_SysLib_Delay(5);
                }
                if (tmp_id != their_id || tmp_id == our_id) {
                    LOG_ERR("ID error, resetting\n");
                    break;
                }
                rcv_bytes = 0;
                for (int j = 0; j < 5; j++) {
                    rcv_bytes += Cy_SCB_UART_GetArray(SCB1, &sendbuf[rcv_bytes], 16 - rcv_bytes);
                    if (rcv_bytes >= 16) break;
                    Cy_SysLib_Delay(5);
                }

                if (rcv_bytes < 16) { LOG_ERR("Received %d bytes\n", rcv_bytes); }
                else { LOG_DEBUG("Received %d bytes\n", rcv_bytes); }

                // Failed
                if (rcv_bytes < 16) continue;

                LOG_DEBUG("Received encrypted block:");
                for (int j = 0; j < 16; j++) LOG_DEBUG_NOFMT(" %02x", sendbuf[j]);
                LOG_DEBUG_NOFMT("\n");

                // Check sent message (now in srcbuf)
                Cy_Crypto_Core_Aes_Ecb(CRYPTO, CY_CRYPTO_DECRYPT, srcbuf, sendbuf, &aes_state);
                int j;
                for (j = 0; j < 16; j++) if (srcbuf[j] != msg_yea[j]) break;

                // If loop was broken prematurely, not every byte passed the check.
                if (j != 16) break;
                failed = 0;
                break;
            }

            if (failed) {
                LOG_WARN("AES key check failed, check connection; resetting\n");
                Cy_Crypto_Core_Aes_Free(CRYPTO, &aes_state);
                Cy_Crypto_Core_Disable(CRYPTO);
                Cy_Crypto_Core_ClearVuRegisters(CRYPTO);
                continue;
            }
        }

        // Real loop
        LOG_INFO("Successfully connected with other PSoC\n");

        char lonely_days = 0;
        LOG_INFO("t: %04x o: %04x\n", their_id, our_id);
        for (;;) {
            LOG_INFO_NOFMT("\n");
            if (lonely_days > 10) {
                LOG_WARN("More than 10 communication failures in a row; resetting\n");
                Cy_Crypto_Core_Aes_Free(CRYPTO, &aes_state);
                Cy_Crypto_Core_Disable(CRYPTO);
                Cy_Crypto_Core_ClearVuRegisters(CRYPTO);
                break;
            }
            // Even with all the previous stuff for detecting feedback, system
            // still does not catch feedback.
            // Final thing should do it
            int num_in_rx_buf = Cy_SCB_UART_GetNumInRxFifo(SCB1);
            if (num_in_rx_buf == 0) {
                Cy_SysLib_Delay(100);
            } else if (num_in_rx_buf < 16) {
                Cy_SysLib_Delay(43);
            }

            if (lonely_days) Cy_GPIO_Inv(GPIO_PRT13, 7);
            else Cy_GPIO_Set(GPIO_PRT13, 7);


            if (!num_in_rx_buf) {
                char next_msg = rangeIntr ? UART_MSG_CONN_VOLTMETER : UART_MSG_CONN_KEEPALIVE;
                rangeIntr = false;

                srcbuf[0] = next_msg;
                switch (next_msg) {
                    case UART_MSG_CONN_UNKNOWN:
                        break;
                    case UART_MSG_CONN_KEEPALIVE:
                        break;
                    case UART_MSG_CONN_VOLTMETER:
                        {
                            int16_t code = Cy_SAR_GetResult16(SAR, 0);
                            float32_t val = Cy_SAR_CountsTo_Volts(SAR, 0, code);
                            for (int i = 0; i < 4; i++) {
                                srcbuf[i + 1] = ((uint8_t*)&val)[i];
                            }
                        }
                        break;
                }

                // last 2 bytes are our id
                ((uint16_t*)srcbuf)[7] = our_id;

                memcpy(ivbuf, our_last_block, 16);

                Cy_Crypto_Core_Aes_Cbc(CRYPTO, CY_CRYPTO_ENCRYPT, 16, ivbuf, sendbuf, srcbuf, &aes_state);
                
                memcpy(our_last_block, sendbuf, 16);

                LOG_DEBUG("Sending ");
                for (int i = 0; i < 16; i++) {
                    LOG_DEBUG_NOFMT("%02x", sendbuf[i]);
                }
                LOG_DEBUG_NOFMT("\n");
                LOG_INFO("\033[1;93mTo TDM:\033[0;93m %s (%02x) with id (%u)\033[m\n", get_uart_msg_type_name(srcbuf[0]), srcbuf[0], our_id);

                Cy_SCB_UART_Put(UART_SCB, UART_MSG_APP);
                Cy_SCB_UART_PutArray(SCB1, sendbuf, 16);

                // Cache sent block into iv for later
                memcpy(ivbuf, sendbuf, 16);
            }

            // Receive encrypted block into `sendbuf'
            int rcv_bytes = 0;
            if (Cy_SCB_UART_GetNumInRxFifo(SCB1)) {
                uint32_t msg = Cy_SCB_UART_Get(UART_SCB);
                if (msg == 0xFFFFFFFF) {
                    LOG_WARN("Reported elements in rx buffer, but get failed\n");
                    lonely_days++;
                    continue;
                }

                if (msg == UART_MSG_APP) {
                    for (int j = 0; j < 5; j++) {
                        rcv_bytes += Cy_SCB_UART_GetArray(UART_SCB, &sendbuf[rcv_bytes], 16 - rcv_bytes);
                        if (rcv_bytes >= 16) break;
                        Cy_SysLib_Delay(5);
                    }

                    // Failed
                    if (rcv_bytes < 16) {
                        LOG_WARN("Less than 16 bytes received in encrypted comms (%d)\n", rcv_bytes);
                        lonely_days += lonely_days + 1;
                        continue;
                    }


                } else if (msg == UART_MSG_RESET || msg == UART_MSG_ADV) {
                    // reset
                    LOG_WARN("External reset requested; resetting\n");
                    Cy_Crypto_Core_Aes_Free(CRYPTO, &aes_state);
                    Cy_Crypto_Core_Disable(CRYPTO);
                    Cy_Crypto_Core_ClearVuRegisters(CRYPTO);
                    break;
                } else {
                    LOG_WARN("Unknown command %02lx\n", msg);
                }
            } else {
                LOG_WARN("No bytes received\n");
                lonely_days += lonely_days + 1;
                continue;
            }

            LOG_DEBUG("Receive ");
            for (int i = 0; i < 16; i++) {
                LOG_DEBUG_NOFMT("%02x", sendbuf[i]);
            }
            LOG_DEBUG_NOFMT("\n");

            // Their message should not be identical to ours!
            int j;
            for (j = 0; j < 16; j++) if (sendbuf[j] != ivbuf[j]) break;
            if (j == 16) {
                LOG_WARN("Received message same as sent message; possible feedback\n");
                // feedback is heresy so it counts as 5 lonely days
                lonely_days += 5;
                continue;
            }

            memcpy(ivbuf, their_last_block, 16);
            memcpy(their_last_block, sendbuf, 16);

            // Check sent message (now in srcbuf)
            Cy_Crypto_Core_Aes_Cbc(CRYPTO, CY_CRYPTO_DECRYPT, 16, ivbuf, srcbuf, sendbuf, &aes_state);

            // Last 2 bytes are ID
            if (((uint16_t*)srcbuf)[7] != their_id) {
                LOG_ERR("Last 2 bytes of received message (%u) isn't their id (%u)", ((uint16_t*)srcbuf)[7], their_id);
                lonely_days++;

                if (((uint16_t*)srcbuf)[7] == our_id) {
                    LOG_ERR_NOFMT(", it's ours!!!\n");
                    lonely_days += 5;

                    continue;
                } else {
                    LOG_ERR_NOFMT("\n");
                }
            }
            LOG_INFO("\033[1;36mFrom TDM:\033[0;36m %s (%02x)\033[m\n", get_uart_msg_type_name(srcbuf[0]), srcbuf[0]);

            switch (srcbuf[0]) {
                case UART_MSG_CONN_KEEPALIVE:
                    lonely_days = 0;
                    break;
                case UART_MSG_CONN_UNKNOWN:
                    lonely_days++;
                    break;
            }
        }
    }

    LOG_INFO("Program end\n");
}

void init_uart_on_scb1() {
    cy_stc_scb_uart_config_t uart_cfg = {
        .uartMode = CY_SCB_UART_STANDARD,
        .oversample = 8,
        .dataWidth = 8,
        .parity = CY_SCB_UART_PARITY_NONE,
        .stopBits = CY_SCB_UART_STOP_BITS_1,
        .breakWidth = 11
    };

    Cy_SCB_UART_Init(SCB1, &uart_cfg, 0);

    Cy_GPIO_Pin_FastInit(GPIO_PRT10, 0, CY_GPIO_DM_HIGHZ, 1, P10_0_SCB1_UART_RX);
    Cy_GPIO_Pin_FastInit(GPIO_PRT10, 1, CY_GPIO_DM_STRONG_IN_OFF, 1, P10_1_SCB1_UART_TX);
    Cy_SysClk_PeriphAssignDivider(PCLK_SCB1_CLOCK, CY_SYSCLK_DIV_8_BIT, 2);
    Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_8_BIT, 2, 54);
    Cy_SysClk_PeriphEnableDivider(CY_SYSCLK_DIV_8_BIT, 2);

    Cy_SCB_UART_Enable(SCB1);
}

#define CASE_RETURN_STR(x) case x: return #x;
const char *get_uart_msg_type_name(uart_msg_type_t name) {
    switch (name) {
        CASE_RETURN_STR(UART_MSG_CONN_KEEPALIVE)
        CASE_RETURN_STR(UART_MSG_CONN_VOLTMETER)
        CASE_RETURN_STR(UART_MSG_CONN_UNKNOWN)
		CASE_RETURN_STR(UART_MSG_ADV)
		CASE_RETURN_STR(UART_MSG_APP)
		CASE_RETURN_STR(UART_MSG_ACK)
		CASE_RETURN_STR(UART_MSG_RESET)
    }
    return "UNKNOWN_MESSAGE";
}
