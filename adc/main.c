#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_pdl.h"
#include "cy_retarget_io.h"
#include "ansi.h"
#include "cycfg.h"
#pragma clang diagnostic pop

int16_t adcMv;
uint16_t adcCode;
bool adcTripped = false;
bool mcwdtTripped = false;

void adc_isr() {
    uint32_t intrStatus = Cy_SAR_GetInterruptStatusMasked(pass_0_sar_0_HW);
    uint32_t rangeIntrStatus = Cy_SAR_GetRangeInterruptStatusMasked(pass_0_sar_0_HW);

    if (rangeIntrStatus & 1) {
        adcTripped = true;
        Cy_SAR_ClearRangeInterrupt(pass_0_sar_0_HW, 1);
        Cy_SAR_SetRangeInterruptMask(pass_0_sar_0_HW, 0);
        Cy_MCWDT_ResetCounters(MCWDT_STRUCT0, CY_MCWDT_CTR1, 63);
        Cy_MCWDT_Enable(MCWDT_STRUCT0, CY_MCWDT_CTR1, 0);
        goto _goto_adcRead;
    }
    if ((intrStatus & CY_SAR_INTR_EOS) && !adcTripped) {

_goto_adcRead:
        adcCode = Cy_SAR_GetResult16(pass_0_sar_0_HW, 0);
        adcMv = Cy_SAR_CountsTo_mVolts(pass_0_sar_0_HW, 0, adcCode);

    }

    Cy_SAR_ClearInterrupt(pass_0_sar_0_HW, intrStatus);
    NVIC_ClearPendingIRQ(pass_0_sar_0_IRQ);
}

void mcwdt_isr() {
    uint32_t intrStatus = Cy_MCWDT_GetInterruptStatusMasked(MCWDT_STRUCT0);

    if (intrStatus & CY_MCWDT_CTR0)
        mcwdtTripped = true;

    if (intrStatus & CY_MCWDT_CTR1) {
        Cy_MCWDT_ResetCounters(MCWDT_STRUCT0, CY_MCWDT_CTR1 | CY_MCWDT_CTR0, 93);
        Cy_MCWDT_Disable(MCWDT_STRUCT0, CY_MCWDT_CTR1, 93);
        Cy_SAR_SetRangeInterruptMask(pass_0_sar_0_HW, 1);
    }

    Cy_MCWDT_ClearInterrupt(MCWDT_STRUCT0, intrStatus);
    NVIC_ClearPendingIRQ(srss_interrupt_mcwdt_0_IRQn);
}

int main() {
    cybsp_init();
    __enable_irq();
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, 115200);
    Cy_GPIO_Pin_FastInit(GPIO_PRT13, 7, CY_GPIO_DM_STRONG_IN_OFF, 1, HSIOM_SEL_GPIO);

    init_cycfg_clocks();
    init_cycfg_peripherals();
    init_cycfg_pins();

    Cy_SysAnalog_Init(&pass_0_aref_0_config);
    Cy_SysAnalog_Enable();
    Cy_SAR_Init(pass_0_sar_0_HW, &pass_0_sar_0_config);
    Cy_SAR_Enable(pass_0_sar_0_HW);
        

    cy_stc_sysint_t adcIntrCfg = {
        .intrSrc = pass_0_sar_0_IRQ,
        .intrPriority = 6,
    };
    Cy_SysInt_Init(&adcIntrCfg, adc_isr);
    NVIC_EnableIRQ(adcIntrCfg.intrSrc);

    cy_stc_mcwdt_config_t mcwdtCfg = {
        .c0Match = 32768,
        .c0Mode = CY_MCWDT_MODE_INT,
        .c0ClearOnMatch = true,
        .c1Match = 3277,
        .c1Mode = CY_MCWDT_MODE_INT,
        .c1ClearOnMatch = true
    };
    Cy_MCWDT_Init(MCWDT_STRUCT0, &mcwdtCfg);
    Cy_MCWDT_SetInterruptMask(MCWDT_STRUCT0, CY_MCWDT_CTR0 | CY_MCWDT_CTR1);
    Cy_MCWDT_Enable(MCWDT_STRUCT0, CY_MCWDT_CTR0, 93);

    cy_stc_sysint_t mcwdtIntrCfg = {
        .intrSrc = srss_interrupt_mcwdt_0_IRQn,
        .intrPriority = 7
    };
    Cy_SysInt_Init(&mcwdtIntrCfg, mcwdt_isr);
    NVIC_EnableIRQ(mcwdtIntrCfg.intrSrc);
    
    Cy_SAR_StartConvert(pass_0_sar_0_HW, CY_SAR_START_CONVERT_CONTINUOUS);

    float maxCode_inv = 1.0f / 0xFFF;
    char buf[256];
    while (1) {
        if (adcTripped || mcwdtTripped) {
            uint16_t code = adcCode;
            int16_t mv = adcMv;

            if (adcTripped) {
                printf("\033[41;97m");
            }
            mcwdtTripped = 0;

            int slen = sprintf(buf, "%4u counts   %4dmV", code, mv);
            while (slen < 32) {
                buf[slen++] = ' ';
            }
            buf[slen++] = '[';

            uint16_t barLen = 32;
            uint16_t num = (code * maxCode_inv) * barLen;
            for (uint16_t i = 0; i < num; i++)
                buf[slen++] = '#';
            for (uint16_t i = num; i < barLen; i++)
                buf[slen++] = ' ';
            buf[slen++] = ']';
            buf[slen++] = 0;

            printf("%s\r\n", buf);

            if (adcTripped) {
                printf("\033[m");
            }
            adcTripped = 0;
        }
    }
}
