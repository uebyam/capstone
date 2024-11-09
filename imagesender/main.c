#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#include "cybsp.h"
#include "cyhal.h"
#include "cy_pdl.h"
#include "cy_retarget_io.h"

#include "dh.h"

#include <ctype.h>
#include <stdlib.h>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "image.h"

bool commscb_trg = false;
bool mcwdt_0trg = false;
bool mcwdt_1trg = false;

uint8_t commscb_buf[256] = {};
uint8_t commscb_reqlen;
uint8_t commscb_rcvlen;
void commscb_isr();
void mcwdt_isr();

void init_uart();
void init_mcwdt();

void commscb_set_level(uint32_t level, bool clear);

int main() {
    cybsp_init();
    __enable_irq();

    cy_retarget_io_init(P5_1, P5_0, 500000);

    printf("\033[m\033[2J\033[H");

    init_uart();
    init_mcwdt();


    bool sender = false;
    bool expectB = false;
    uint16_t sharedKey;

_goto_advLoop:
    {
        Cy_MCWDT_ResetCounters(MCWDT_STRUCT0, CY_MCWDT_CTR0, 0);
        Cy_MCWDT_SetMatch(MCWDT_STRUCT0, 0, 65535, 0);

        while (1) {
            commscb_set_level(1, true);
            cyhal_syspm_sleep();
            
            if (commscb_trg) {
                commscb_trg = false;

                switch (commscb_buf[0]) {
                    case 'A':
                        printf("Received A: sending B, waiting for B and going to TRNG\n\n");
                        expectB = true;
                        goto _goto_trngLoop;
                        break;

                    case 'B':
                        printf("Received B: sending B and going to TRNG\n");

                        goto _goto_trngLoop;

                    default:
                        printf("Got %c (%02x)\n", commscb_buf[0], commscb_buf[0]);
                }
            }
            if (mcwdt_0trg) {
                mcwdt_0trg = false;

                Cy_SCB_UART_Put(commscb_HW, 'A');
                printf("Sending A\n");
            }
        }
    }


_goto_trngLoop:
    {
        cyhal_trng_t trng;
        cyhal_trng_init(&trng);

        Cy_MCWDT_ResetCounters(MCWDT_STRUCT0, CY_MCWDT_CTR0, 0);
        Cy_MCWDT_SetMatch(MCWDT_STRUCT0, 0, 32767, 0);

        for (int counts = 0; counts < 10; counts++, expectB = true) {
            char buf[16] = {};
            uint32_t randNum = cyhal_trng_generate(&trng);
            Cy_SCB_UART_Put(commscb_HW, 'B');
            if (expectB) {
                Cy_MCWDT_ResetCounters(MCWDT_STRUCT0, CY_MCWDT_CTR0, 0);
                Cy_MCWDT_SetMatch(MCWDT_STRUCT0, 0, 32767, 0);

                commscb_set_level(1, false);
                if (!commscb_trg) cyhal_syspm_sleep();

                if (commscb_trg) {
                    commscb_trg = false;
                } else continue;
            }
            Cy_SCB_UART_PutArray(commscb_HW, &randNum, 4);
            printf("iter %d: Our random number: %lu\n", counts, randNum);

            commscb_set_level(4, false);
            if (!commscb_trg) cyhal_syspm_sleep();

            if (commscb_trg) {
                commscb_trg = false;

                uint32_t theirRandNum = ((uint32_t*)commscb_buf)[0];
                printf("iter %d: The random number: %lu\n", counts, theirRandNum);

                if (theirRandNum == randNum) {
                    continue;
                }
                
                if (randNum > theirRandNum) sender = true;
                // sender = true;

                printf("Success\n\n");

                cyhal_trng_free(&trng);

                goto _goto_dh;
            }
        }

        printf("Failure\n\n");

        cyhal_trng_free(&trng);

        goto _goto_advLoop;
    }


_goto_dh:
    {
        cyhal_trng_t trng;
        cyhal_trng_init(&trng);

        Cy_MCWDT_ResetCounters(MCWDT_STRUCT0, CY_MCWDT_CTR0, 0);
        Cy_MCWDT_SetMatch(MCWDT_STRUCT0, 0, 32767, 0);

        for (int counts = 0; counts < 5; counts++) {
            commscb_set_level(2, false);
            uint16_t ourSecret = cyhal_trng_generate(&trng) & 0xFFFF;

            uint16_t ourPublic;
            dh_compute_public_key(&ourPublic, &ourSecret, 1, DH_DEFAULT_MOD, DH_DEFAULT_GENERATOR);

            printf("Sending public: %u\n", ourPublic);

            Cy_SCB_UART_PutArrayBlocking(commscb_HW, &ourPublic, 2);

            if (!commscb_trg) cyhal_syspm_sleep();

            if (commscb_trg) {
                commscb_trg = false;

                uint16_t theirPublic = *(uint16_t*)commscb_buf;
                printf("Received public: %u\n", theirPublic);

                dh_compute_shared_secret(&sharedKey, &ourSecret, &theirPublic, 1, DH_DEFAULT_MOD);
                
                printf("Shared key: %u\n\n", sharedKey);

                cyhal_trng_free(&trng);
                goto _goto_sendImg;
            }
        }

        printf("Failure\n\n");
        cyhal_trng_free(&trng);
        goto _goto_advLoop;
    }


_goto_sendImg:
    {
        if (sender) {
            printf("Sending\n");
            Cy_SCB_UART_PutArrayBlocking(commscb_HW, &imageWidth, 2);
            Cy_SCB_UART_PutArrayBlocking(commscb_HW, &imageHeight, 2);

            uint16_t key[8] = {sharedKey};
            char iv[16] = "iv  iv  iv  iv  ";

            char srcbuf[16];
            char encbuf[16];

            cy_stc_crypto_aes_state_t aesState;
            Cy_Crypto_Core_Enable(CRYPTO);
            Cy_Crypto_Core_Aes_Init(CRYPTO, key, CY_CRYPTO_KEY_AES_128, &aesState);
            Cy_Crypto_Core_Aes_Cbc_Set_IV(CRYPTO, iv, &aesState);
            Cy_Crypto_Core_Aes_Cbc_Setup(CRYPTO, CY_CRYPTO_ENCRYPT, &aesState);

            uint32_t bytes = imageWidth * imageHeight;
            uint32_t blockIndex = 0;
            int wg = 0;
            for (int32_t i = bytes - 16; i >= 0; i -= 16) {
                memcpy(srcbuf, &(image[i]), 16);
                for (int g = 0; g < 8; g++) {
                    int tmp = srcbuf[g];
                    srcbuf[g] = srcbuf[15 - g];
                    srcbuf[15 - g] = tmp;
                }
                Cy_Crypto_Core_Aes_Cbc_Update(CRYPTO, 16, encbuf, srcbuf, &aesState);
                Cy_SCB_UART_PutArrayBlocking(commscb_HW, encbuf, 16);

                char sendbuf[256];
                for (int g = 15; g >= 0; g--) {
                    int sendsize = snprintf(sendbuf, 256, "\033[48;5;%um ", image[i + g]);
                    Cy_SCB_UART_PutArrayBlocking(SCB5, sendbuf, sendsize);
                    wg++;
                    if (wg >= imageWidth) {
                        wg = 0;
                        Cy_SCB_UART_PutString(SCB5, "\033[m\r\n");
                    }
                }
                
                Cy_SysLib_Delay(100);
                blockIndex++;
            }

            Cy_Crypto_Core_Aes_Cbc_Finish(CRYPTO, &aesState);
            Cy_Crypto_Core_Aes_Free(CRYPTO, &aesState);
            Cy_Crypto_Core_Disable(CRYPTO);
            Cy_Crypto_Core_ClearVuRegisters(CRYPTO);

            for (;;);
        } else {
            uint16_t width, height;
            Cy_SCB_UART_GetArrayBlocking(commscb_HW, &width, 2);
            Cy_SCB_UART_GetArrayBlocking(commscb_HW, &height, 2);

            uint16_t key[8] = {sharedKey};
            char iv[16] = "iv  iv  iv  iv  ";

            uint8_t buf[16], srcbuf[16];

            cy_stc_crypto_aes_state_t aesState;
            Cy_Crypto_Core_Enable(CRYPTO);
            Cy_Crypto_Core_Aes_Init(CRYPTO, key, CY_CRYPTO_KEY_AES_128, &aesState);
            Cy_Crypto_Core_Aes_Cbc_Set_IV(CRYPTO, iv, &aesState);
            Cy_Crypto_Core_Aes_Cbc_Setup(CRYPTO, CY_CRYPTO_DECRYPT, &aesState);

            int wg = 0;
            while (1) {
                Cy_SCB_UART_GetArrayBlocking(commscb_HW, buf, 16);
                Cy_Crypto_Core_Aes_Cbc_Update(CRYPTO, 16, srcbuf, buf, &aesState);
                
                char sendbuf[256];
                for (int g = 0; g < 16; g++) {
                    int sendsize = snprintf(sendbuf, 256, "\033[48;5;%um ", srcbuf[g]);
                    Cy_SCB_UART_PutArrayBlocking(SCB5, sendbuf, sendsize);
                    wg++;
                    if (wg >= imageWidth) {
                        wg = 0;
                        Cy_SCB_UART_PutString(SCB5, "\033[m\r\n");
                    }
                }
            }
            for (;;);
        }
    }
}

void init_uart() {
    Cy_SCB_UART_Init(commscb_HW, &commscb_config, 0);
    Cy_SCB_UART_Enable(commscb_HW);

    commscb_reqlen = 1;
    Cy_SCB_SetRxFifoLevel(commscb_HW, commscb_reqlen - 1);
    Cy_SCB_SetRxInterruptMask(commscb_HW, CY_SCB_RX_INTR_LEVEL);
    
    cy_stc_sysint_t commscb_intrConfig = {
        .intrSrc = commscb_IRQ,
        .intrPriority = 6
    };
    Cy_SysInt_Init(&commscb_intrConfig, commscb_isr);
    NVIC_EnableIRQ(commscb_IRQ);
}

void init_mcwdt() {
    cy_stc_mcwdt_config_t mcwdt0cfg = {
        .c0Match = 65535,
        .c0Mode = CY_MCWDT_MODE_INT,
        .c0ClearOnMatch = true
    };
    Cy_MCWDT_Init(MCWDT_STRUCT0, &mcwdt0cfg);
    Cy_MCWDT_SetInterruptMask(MCWDT_STRUCT0, CY_MCWDT_CTR0);
    Cy_MCWDT_Enable(MCWDT_STRUCT0, CY_MCWDT_CTR0, 0);

    cy_stc_sysint_t mcwdt0intrCfg = {
        .intrSrc = srss_interrupt_mcwdt_0_IRQn,
        .intrPriority = 7
    };
    Cy_SysInt_Init(&mcwdt0intrCfg, mcwdt_isr);
    NVIC_EnableIRQ(mcwdt0intrCfg.intrSrc);
}

void commscb_isr() {
    uint32_t mask = Cy_SCB_GetRxInterruptStatusMasked(commscb_HW);
    commscb_rcvlen = Cy_SCB_UART_GetArray(commscb_HW, commscb_buf, commscb_reqlen);
    Cy_SCB_ClearRxInterrupt(commscb_HW, mask);
    Cy_SCB_SetRxInterruptMask(commscb_HW, 0);

    (void)Cy_SCB_GetRxInterruptMask(commscb_HW);

    if (mask & CY_SCB_RX_INTR_LEVEL) commscb_trg = true;


    NVIC_ClearPendingIRQ(commscb_IRQ);
}

void mcwdt_isr() {
    uint32_t mask = Cy_MCWDT_GetInterruptStatusMasked(MCWDT_STRUCT0);

    if (mask & CY_MCWDT_CTR0) {
        mcwdt_0trg = true;
    }
    if (mask & CY_MCWDT_CTR1) {
        mcwdt_1trg = true;
    }
    
    Cy_MCWDT_ClearInterrupt(MCWDT_STRUCT0, mask);
    NVIC_ClearPendingIRQ(srss_interrupt_mcwdt_0_IRQn);
}

void commscb_set_level(uint32_t level, bool clear) {
    commscb_reqlen = level;
    commscb_rcvlen = 0;
    Cy_SCB_SetRxFifoLevel(commscb_HW, commscb_reqlen - 1);
    commscb_trg = false;
    if (clear) Cy_SCB_ClearRxFifo(commscb_HW);
    Cy_SCB_ClearRxInterrupt(commscb_HW, CY_SCB_RX_INTR_LEVEL);
    Cy_SCB_SetRxInterruptMask(commscb_HW, CY_SCB_RX_INTR_LEVEL);
}
