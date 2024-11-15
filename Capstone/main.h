#ifndef MAIN_H
#define MAIN_H

#include "FreeRTOS.h"
#include "task.h"
#include <inttypes.h>

TaskHandle_t get_ess_handle(void);

// TODO: refactor this function the naming is dangerous
void start_bt(void);
void global_start_advertisement(void);
void global_stop_advertisement(void);

extern uint8_t global_bt_page;
#define BT_PAGE_SIZE (20)
#define USE_SPACER_BYTE (1u)


#if USE_SPACER_BYTE
#define GLOBAL_BT_PAGE_SIZE (BT_PAGE_SIZE - USE_SPACER_BYTE)
#else
#define GLOBAL_BT_PAGE_SIZE (BT_PAGE_SIZE)
#endif

extern bool global_bluetooth_started;
extern bool global_bluetooth_enabled;
extern bool global_bluetooth_connected;
extern char global_advertisement_state;
extern bool global_rtc_set;

void reset_tampers(void);

#endif
