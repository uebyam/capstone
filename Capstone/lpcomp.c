#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "lpcomp.h"

#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>
#include <queue.h>

#include "eeprom.h"
#include "main.h"
#pragma clang diagnostic pop

void comp_isr();
void timmy_isr();

void comp_task(void*);
void timer_task(void*);

TaskHandle_t lpcomp_task_handle;
TaskHandle_t timer_task_handle;

int wtf = 0;

int getWtf() {
    return wtf;
}

const cy_stc_lpcomp_config_t comp_config = {
    .outputMode    =  CY_LPCOMP_OUT_DIRECT,
    .hysteresis    =  CY_LPCOMP_HYST_ENABLE,
    .power         =  CY_LPCOMP_MODE_LP,
    .intType       =  CY_LPCOMP_INTR_BOTH
};

const cy_stc_sysint_t interrupt_config = {
    .intrSrc       =  lpcomp_interrupt_IRQn,
    .intrPriority  =  7
};

void init_lpcomp() {
    BaseType_t rtos_result;
    Cy_LPComp_Init(LPCOMP, CY_LPCOMP_CHANNEL_0, &comp_config);
    Cy_LPComp_SetInputs(
        LPCOMP, CY_LPCOMP_CHANNEL_0,
        CY_LPCOMP_SW_GPIO, CY_LPCOMP_SW_GPIO
    );

    Cy_LPComp_Enable(LPCOMP, CY_LPCOMP_CHANNEL_0);

    // interrupts
    Cy_SysLib_Delay(1);

    Cy_LPComp_SetInterruptMask(LPCOMP, 1);

    // stupid rtos
    rtos_result = xTaskCreate(comp_task, "LPComp Task", (configMINIMAL_STACK_SIZE * 4),
                              NULL, (configMAX_PRIORITIES - 3), &lpcomp_task_handle);

    if (rtos_result == pdPASS) {
        printf("LPComp task created\r\n");
        Cy_SysInt_Init(&interrupt_config, comp_isr);
        NVIC_EnableIRQ(interrupt_config.intrSrc);
    }
    else printf("LPComp task creation failed\r\n");
}

void comp_isr() {
    Cy_LPComp_ClearInterrupt(LPCOMP, 1);
    NVIC_ClearPendingIRQ(lpcomp_interrupt_IRQn);

    BaseType_t xHigherPriorityTaskWoken;
    xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(lpcomp_task_handle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

    vTaskNotifyGiveFromISR(get_ess_handle(), &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void comp_task(void *pvParam) {
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        printf("Comparator task\r\n");

        increaseTamperCount();
    }
}

void timer_task(void *p) {
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        printf("Timer task! %d\r\n", wtf);
    }
}

void timmy_isr(void) {
    Cy_MCWDT_ClearInterrupt(MCWDT_STRUCT0, CY_MCWDT_CTR0);
    NVIC_ClearPendingIRQ(srss_interrupt_mcwdt_0_IRQn);

    BaseType_t xHigherPriorityTaskWoken;
    xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(timer_task_handle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

    wtf++;
}
