#ifndef MAIN_H
#define MAIN_H

#include "FreeRTOS.h"
#include "task.h"

TaskHandle_t get_ess_handle(void);

// TODO: refactor this function the naming is dangerous
void start_bt(void);
void global_start_advertisement(void);

extern bool global_bluetooth_started;

#endif
