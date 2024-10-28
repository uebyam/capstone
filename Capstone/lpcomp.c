#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif
#include "lpcomp.h"

#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>
#include <queue.h>

#include "eepromManager.h"
#include "main.h"
#ifdef clang
#pragma clang diagnostic pop
#endif


void comp_isr();
void timer_callback(void*, cyhal_lptimer_event_t);

void comp_task(void*);
void timer_task(void*);

TaskHandle_t lpcomp_task_handle;
TaskHandle_t timer_task_handle;

const cy_stc_lpcomp_config_t comp_config = {
    .outputMode    =  CY_LPCOMP_OUT_DIRECT,
    .hysteresis    =  CY_LPCOMP_HYST_ENABLE,
    .power         =  CY_LPCOMP_MODE_ULP,
    .intType       =  CY_LPCOMP_INTR_FALLING
};

const cy_stc_sysint_t interrupt_config = {
    .intrSrc       =  lpcomp_interrupt_IRQn,
    .intrPriority  =  7
};

cyhal_lptimer_t lptimer;
char lptimer_running = 0;

void init_lpcomp(char rtos) {
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


    if (rtos) {
        // stupid rtos
        rtos_result = xTaskCreate(
                comp_task, "LPComp Task", (configMINIMAL_STACK_SIZE * 4),
                NULL, (configMAX_PRIORITIES - 3), &lpcomp_task_handle
                );

        if (rtos_result == pdPASS) {
            printf("LPComp task created\r\n");
            Cy_SysInt_Init(&interrupt_config, comp_isr);
            NVIC_ClearPendingIRQ(interrupt_config.intrSrc);
            NVIC_EnableIRQ(interrupt_config.intrSrc);
        } else {
            printf("LPComp task creation failed\r\n");
        }


        rtos_result = xTaskCreate(
                timer_task, "Drop Detection Task", (configMINIMAL_STACK_SIZE * 4),
                NULL, (configMAX_PRIORITIES - 3), &timer_task_handle
                );

        if (rtos_result == pdPASS) {
            printf("Timer task created\r\n");

            cyhal_lptimer_init(&lptimer);
            cyhal_lptimer_register_callback(&lptimer, timer_callback, &lptimer);
            cyhal_lptimer_enable_event(&lptimer, CYHAL_LPTIMER_COMPARE_MATCH, 4, 1);
        } else {
            printf("Timer task creation failed\r\n");
        }
    }
}

void comp_isr() {
    Cy_LPComp_ClearInterrupt(LPCOMP, 1);
    NVIC_ClearPendingIRQ(lpcomp_interrupt_IRQn);

    BaseType_t xHigherPriorityTaskWoken;
    xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(lpcomp_task_handle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void comp_task(void *pvParam) {
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        printf("Possible tamper detected, waiting for drop detection\r\n");

        if (!lptimer_running) {
            lptimer_running = 1;
            cyhal_lptimer_set_delay(&lptimer, 8192);
        }
    }
}

void timer_callback(void *refcon, cyhal_lptimer_event_t event) {
    BaseType_t xHigherPriorityTaskWoken;
    xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(timer_task_handle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void timer_task(void *p) {
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (!lptimer_running) continue;

        if (Cy_LPComp_GetCompare(LPCOMP, CY_LPCOMP_CHANNEL_0) == 0) {
            printf("Detected tamper!\r\n");
            increaseTamperCount();

            if (global_bluetooth_started) {
                xTaskNotifyGive(get_ess_handle());
            }
        } else {
            printf("Did not detect tamper\r\n");
        }

        lptimer_running = 0;
    }
}
