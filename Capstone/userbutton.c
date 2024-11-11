#include "userbutton.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif
#include "cybsp.h"
#include "cyhal.h"
#include "eepromManager.h"
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "ansi.h"
#include "uart.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif

void userbutton_isr(void);
void userbutton_task(void*);

TaskHandle_t userbutton_task_handle;

char times_pressed = 0;

void init_userbutton() {
    // TODO
    Cy_GPIO_Pin_FastInit(GPIO_PRT0, 4, CY_GPIO_DM_PULLUP, 1, P0_4_GPIO);
    Cy_GPIO_SetInterruptMask(GPIO_PRT0, 4, 1);
    Cy_GPIO_SetInterruptEdge(GPIO_PRT0, 4, CY_GPIO_INTR_FALLING);
    
    BaseType_t rtos_rslt = xTaskCreate(
            userbutton_task, "User button", (configMINIMAL_STACK_SIZE * 4),
            0, configMAX_PRIORITIES - 3, &userbutton_task_handle
            );

    if (rtos_rslt != pdPASS) {
        LOG_ERR("User button task creation failed\r\n");
        return;
    } else {
        LOG_DEBUG("Successfully created user button task\r\n");
    }
}

void init_userbutton_intr() {
    cy_stc_sysint_t gpio_intr_cfg = {
        .intrSrc = ioss_interrupts_gpio_0_IRQn,
        .intrPriority = 7
    };

    Cy_SysInt_Init(&gpio_intr_cfg, userbutton_isr);
    NVIC_EnableIRQ(gpio_intr_cfg.intrSrc);
}

void userbutton_isr() {
    Cy_GPIO_ClearInterrupt(GPIO_PRT0, 4);
    NVIC_ClearPendingIRQ(ioss_interrupts_gpio_0_IRQn);

    BaseType_t higherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(userbutton_task_handle, &higherPriorityTaskWoken);
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

void userbutton_task(void *refcon) {
    init_userbutton_intr();

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (!global_bluetooth_started) {
            start_bt();
        } else if (global_bluetooth_enabled) {
            LOG_DEBUG("Starting advertisements from user button\r\n");
            global_start_advertisement();
        }
    }
}
