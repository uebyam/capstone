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

void init_userbutton() {
    // TODO
    Cy_GPIO_Pin_FastInit(GPIO_PRT0, 4, CY_GPIO_DM_PULLUP, 1, P0_4_GPIO);
}
