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
#include "ansi.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif


void comp_isr();
void timer_callback(void*, cyhal_lptimer_event_t);

void comp_task(void*);

TaskHandle_t lpcomp_task_handle;

const cy_stc_lpcomp_config_t comp_config = {
    .outputMode    =  CY_LPCOMP_OUT_DIRECT,
    .hysteresis    =  CY_LPCOMP_HYST_ENABLE,
    .power         =  CY_LPCOMP_MODE_ULP,
    .intType       =  CY_LPCOMP_INTR_BOTH
};

const cy_stc_sysint_t interrupt_config = {
    .intrSrc       =  lpcomp_interrupt_IRQn,
    .intrPriority  =  7
};

cyhal_lptimer_t lptimer;
char lptimer_running = 0;
char lpcomp_triggered = 0;
char lptimer_expired = 0;

void init_lpcomp(char rtos) {
    Cy_GPIO_Pin_FastInit(&(GPIO->PRT[LPCOMP_PCBPOWER_PRT]), LPCOMP_PCBPOWER_PIN, CY_GPIO_DM_STRONG_IN_OFF, 1, HSIOM_SEL_GPIO);

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
        rtos_result = xTaskCreate(
                comp_task, "LPComp Task", (configMINIMAL_STACK_SIZE * 4),
                NULL, (configMAX_PRIORITIES - 3), &lpcomp_task_handle
                );

        if (rtos_result == pdPASS) {
            LOG_DEBUG("Successfully created LPComp task\r\n");
            Cy_SysInt_Init(&interrupt_config, comp_isr);
            NVIC_ClearPendingIRQ(interrupt_config.intrSrc);
            NVIC_EnableIRQ(interrupt_config.intrSrc);
        } else {
            LOG_ERR("LPComp task creation failed\r\n");
        }
    }
}

void comp_isr() {
    lpcomp_triggered = 1 == Cy_LPComp_GetCompare(LPCOMP, CY_LPCOMP_CHANNEL_0);
    Cy_LPComp_ClearInterrupt(LPCOMP, 1);
    NVIC_ClearPendingIRQ(lpcomp_interrupt_IRQn);

    BaseType_t xHigherPriorityTaskWoken;
    xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(lpcomp_task_handle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void comp_task(void *pvParam) {
    cyhal_lptimer_init(&lptimer);
    cyhal_lptimer_register_callback(&lptimer, timer_callback, &lptimer);
    cyhal_lptimer_enable_event(&lptimer, CYHAL_LPTIMER_COMPARE_MATCH, 4, 1);
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (lpcomp_triggered) {
            if (lptimer_expired) {
                lptimer_expired = 0;

                LOG_INFO("\033[;1;97;48;5;196mTamper detected:\033[;1;38;5;196m Battery lifted\033[m\n");

                increaseTamperCount(EEPROM_TAMPER_TYPE_BATTERY_LIFT);
                xTaskNotifyGive(get_ess_handle());
            } else if (!lptimer_running) {
                lptimer_running = 1;

                cyhal_lptimer_set_delay(&lptimer, 1638);

                LOG_DEBUG("Possible tamper detected, waiting for drop detection\r\n");
            }
        } else {
            if (lptimer_running) {
                lptimer_running = 0;

                LOG_DEBUG("Tamper not detected\n");
            }
        }
    }
}

void timer_callback(void *refcon, cyhal_lptimer_event_t event) {
    if (lptimer_running) lptimer_expired = 1;
    lptimer_running = 0;

    BaseType_t xHigherPriorityTaskWoken;
    xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(lpcomp_task_handle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
