#include "userbutton.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "cybsp.h"
#include "cyhal.h"
#include "eeprom.h"
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#pragma clang diagnostic pop

void userbutton_callback(void *arg, cyhal_gpio_event_t event);
void userbutton_task(void *arg);

TaskHandle_t userbutton_task_handle;

cyhal_gpio_callback_data_t callback_data = {
    .callback = userbutton_callback,
    .callback_arg = 0
};

void init_userbutton() {
    cyhal_gpio_init(CYBSP_USER_BTN, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_PULLUP, 1);
    cyhal_gpio_register_callback(CYBSP_USER_BTN, &callback_data);
    cyhal_gpio_enable_event(CYBSP_USER_BTN, CYHAL_GPIO_IRQ_FALL, 3, true);

    printf("User button hibernation wake set up\r\n");
}

void userbutton_callback(void *arg, cyhal_gpio_event_t event) {
    
}
