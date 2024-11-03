#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#include "cyhal.h"
#include "cybsp.h"
#include "cy_pdl.h"
#include "cy_retarget_io.h"

#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>

#include "cy_em_eeprom.h"

#include "ansi.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

// EEPROM config
// CY_FLASH_SIZEOF_ROW is 512 bytes for CY8C624ABZI-S2D44
//                                     (CY8CPROTO-062-4343W)
#define DATA_SIZE CY_FLASH_SIZEOF_ROW

CY_SECTION(".cy_em_eeprom")
CY_ALIGN(CY_FLASH_SIZEOF_ROW) 
const uint8_t storage[CY_EM_EEPROM_GET_PHYSICAL_SIZE(
        DATA_SIZE,
        1,                      // simple mode
        2,                      // wear leveling level (ignored)
        0                       // redundant copy (ignored)
)] = {};


// normal
uint32_t lvdCount = 0;

TaskHandle_t lvdTaskHandle;
void lvd_task(void*);
void lvd_isr(void);

cy_stc_eeprom_context_t eepromContext;

const char *get_em_eeprom_status_name(cy_en_em_eeprom_status_t eepromStatus);


int main() {
    cybsp_init();
    __enable_irq();
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, 115200);


    // init eeprom
    cy_stc_eeprom_config_t eepromConfig = {
        .eepromSize = DATA_SIZE,
        .simpleMode = 1,
        .wearLevelingFactor = 1,    // ignored
        .blockingWrite = 0,
        .userFlashStartAddr = (uint32_t)&storage
    };

    cy_en_em_eeprom_status_t eepromStatus = Cy_Em_EEPROM_Init(&eepromConfig, &eepromContext);
    if (eepromStatus != CY_EM_EEPROM_SUCCESS) {
        LOG_FATAL("EmEEPROM initialisation failed with %s\n", get_em_eeprom_status_name(eepromStatus));
        Cy_SysLib_Delay(10);
        while (1) Cy_SysPm_DeepSleep(CY_SYSPM_WAIT_FOR_INTERRUPT);
    }

    LOG_DEBUG("EmEEPROM initialisation success\n");

    eepromStatus = Cy_Em_EEPROM_Read(0, &lvdCount, sizeof(lvdCount), &eepromContext);
    if (eepromStatus != CY_EM_EEPROM_SUCCESS) {
        LOG_ERR("EmEEPROM read failed with %s\n", get_em_eeprom_status_name(eepromStatus));
    }

    LOG_INFO("Current LVD count: %lu\n", lvdCount);


    // init lvd
    Cy_LVD_ClearInterruptMask();
    Cy_LVD_SetThreshold(CY_LVD_THRESHOLD_3_1_V);
    Cy_LVD_SetInterruptConfig(CY_LVD_INTR_FALLING);
    Cy_LVD_Enable();
    Cy_SysLib_DelayUs(20);
    Cy_LVD_ClearInterrupt();    // LVD might trigger false interrupt, clear
    
    cy_stc_sysint_t lvdIntrCfg = {
        .intrSrc = srss_interrupt_IRQn,
        .intrPriority = configMAX_SYSCALL_INTERRUPT_PRIORITY
    };
    Cy_SysInt_Init(&lvdIntrCfg, lvd_isr);
    NVIC_EnableIRQ(lvdIntrCfg.intrSrc);

    // init freertos
    BaseType_t rtosStatus = xTaskCreate(lvd_task, "LVD Task", configMINIMAL_STACK_SIZE * 4, 0, 0, &lvdTaskHandle);
    if (rtosStatus != pdPASS) {
        LOG_FATAL("LVD task creation failed\n");
        Cy_SysLib_Delay(10);
        while (1) Cy_SysPm_DeepSleep(CY_SYSPM_WAIT_FOR_INTERRUPT);
    }

    LOG_DEBUG("Starting FreeRTOS scheduler\n");

    vTaskStartScheduler();
}


void lvd_isr() {
    Cy_LVD_ClearInterrupt();
    NVIC_ClearPendingIRQ(srss_interrupt_IRQn);

    BaseType_t t = pdFALSE;
    if (Cy_LVD_GetInterruptStatusMasked()) {
        vTaskNotifyGiveFromISR(lvdTaskHandle, &t);
    }

    portYIELD_FROM_ISR(t);
}


void lvd_task(void *arg) {
    Cy_LVD_SetInterruptMask();
    (void)arg;

    cy_en_em_eeprom_status_t status;

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        lvdCount++;
        status = Cy_Em_EEPROM_Write(0, &lvdCount, sizeof lvdCount, &eepromContext);

        if (status != CY_EM_EEPROM_SUCCESS) {
            LOG_ERR("Writing LVD count failed with %s\n", get_em_eeprom_status_name(status));
        }

        LOG_INFO("LVD count increased to %lu\n", lvdCount);
    }
}


#define CASE_RETURN_STR(x) case x: return #x;
const char *get_em_eeprom_status_name(cy_en_em_eeprom_status_t eepromStatus) {
    switch (eepromStatus) {
        CASE_RETURN_STR(CY_EM_EEPROM_SUCCESS)
        CASE_RETURN_STR(CY_EM_EEPROM_BAD_PARAM)
        CASE_RETURN_STR(CY_EM_EEPROM_BAD_CHECKSUM)
        CASE_RETURN_STR(CY_EM_EEPROM_BAD_DATA)
        CASE_RETURN_STR(CY_EM_EEPROM_WRITE_FAIL)
        CASE_RETURN_STR(CY_EM_EEPROM_REDUNDANT_COPY_USED)
    }
    return "UNKNOWN_STATUS";
}
