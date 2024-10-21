#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "cybt_platform_trace.h"
#include "cyhal.h"
#include "cyhal_gpio.h"
#include "stdio.h"
#include "cyabs_rtos.h"
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <string.h>
#include <timers.h>
#include "GeneratedSource/cycfg_gatt_db.h"
#include "app_bt_gatt_handler.h"
#include "app_bt_utils.h"
#include "wiced_bt_ble.h"
#include "wiced_bt_uuid.h"
#include "wiced_memory.h"
#include "wiced_bt_stack.h"
#include "cycfg_bt_settings.h"
#include "cycfg_gap.h"
#include "cybsp_bt_config.h"
#include "cy_em_eeprom.h"
#pragma clang diagnostic pop

#include "main.h"
#include "eeprom.h"
#include "lpcomp.h"
#include "userbutton.h"

#ifndef ABS
#define ABS(N) ((N < 0) ? (-N) : (N))
#endif

#define IS_NOTIFIABLE(conn_id, cccd) (((conn_id) != 0) ? (cccd) & GATT_CLIENT_CONFIG_INDICATION : 0)

volatile int uxTopUsedPriority;
extern const wiced_bt_cfg_settings_t wiced_bt_cfg_settings;

TaskHandle_t ess_task_handle;

uint16_t app_bt_conn_id;

static wiced_bt_dev_status_t app_bt_management_callback(wiced_bt_management_evt_t event, wiced_bt_management_evt_data_t *p_event_data);
static wiced_result_t app_bt_set_advertisement_data(void);
static void bt_app_init(void);
static void app_start_advertisement(void);

void ess_task(void *pvParam);
void ess_timer_callb(void *callback_arg, cyhal_lptimer_event_t event);

// got no header file for this so i'm putting extern here as a message
extern void idle_task();

int main(void) {
    if (Cy_SysPm_IoIsFrozen())
        Cy_SysPm_IoUnfreeze();

    bool didHib = false;
    if (Cy_SysLib_GetResetReason() & CY_SYSLIB_RESET_HIB_WAKEUP) {
        Cy_SysLib_ClearResetReason();
        didHib = true;
    }

    CY_ASSERT(CY_RSLT_SUCCESS == cybsp_init());
    __enable_irq();
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);

    init_userbutton();
    init_lpcomp(1);
    initEEPROM();

    uxTopUsedPriority = configMAX_PRIORITIES - 1;
    wiced_result_t wiced_result;
    BaseType_t rtos_result;

    cyhal_gpio_init(CONNECTION_LED, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);

    printf("****** Tamper Sensing Service ******\n");

    cybt_platform_config_init(&cybsp_bt_platform_cfg);
    wiced_result = wiced_bt_stack_init(app_bt_management_callback, &wiced_bt_cfg_settings);

    if (WICED_BT_SUCCESS == wiced_result) {
        printf("Bluetooth Stack Initialization Successful\n");
    } else {
        printf("Bluetooth Stack Initialization failed!!\n");
    }

    rtos_result = xTaskCreate(ess_task, "ESS Task", (configMINIMAL_STACK_SIZE * 4),
                              NULL, (configMAX_PRIORITIES - 3), &ess_task_handle);

    if (pdPASS == rtos_result) {
        printf("ESS task created successfully\n");
    } else {
        printf("ESS task creation failed\n");
    }

    uint16_t tamper_count = getTamperCount();

    printf("Current tamper count: %u\n", tamper_count);

    vTaskStartScheduler();

    // Should never get here
    CY_ASSERT(0);
}


static wiced_result_t app_bt_management_callback(wiced_bt_management_evt_t event, wiced_bt_management_evt_data_t *p_event_data) {
    printf("BLUETOOTH --> Event: ");
    printf("%s", get_btm_event_name(event));

    switch (event) {

    case BTM_ENABLED_EVT:
        printf("\nBLUETOOTH --> Device name: %s\n", app_gap_device_name);

        printf("BLUETOOTH --> ");
        print_local_bd_address();

        bt_app_init();
        break;

    case BTM_DISABLED_EVT:
        printf("BLUETOOTH --> Bluetooth disabled\n");
        break;

    case BTM_BLE_ADVERT_STATE_CHANGED_EVT:
        printf("BLUETOOTH --> Advertisement state changed to %s\n",
                get_btm_advert_mode_name(p_event_data->ble_advert_state_changed));
        break;

    default:
        printf(" (!!! unhandled !!!)\n");
        break;
    }

    return WICED_ERROR;
}

static void bt_app_init(void) {
    wiced_bt_gatt_status_t gatt_status = WICED_BT_GATT_ERROR;
    cy_rslt_t rslt;

    gatt_status = wiced_bt_gatt_register(app_bt_gatt_event_callback);
    printf("BLUETOOTH --> [bt_app_init] gatt_register status: %s\n", get_gatt_status_name(gatt_status));

    gatt_status = wiced_bt_gatt_db_init(gatt_database, gatt_database_len, NULL);
    if (WICED_BT_GATT_SUCCESS != gatt_status)
        printf("BLUETOOTH --> [bt_app_init] GATT DB Initialization err 0x%x\n", gatt_status);

    app_start_advertisement();
}

static void app_start_advertisement(void) {
    wiced_result_t wiced_status;

    wiced_status = app_bt_set_advertisement_data();
    if (WICED_SUCCESS != wiced_status)
        printf("BLUETOOTH --> [app_start_advertisement] Raw advertisement failed err 0x%x\n", wiced_status);

    wiced_bt_set_pairable_mode(WICED_FALSE, FALSE);

    wiced_status = wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH, BLE_ADDR_PUBLIC, NULL);
    if (WICED_SUCCESS != wiced_status) {
        printf("BLUETOOTH --> [app_start_advertisement] Starting undirected Bluetooth LE advertisements failed err 0x%x\n", wiced_status);
    }
}

static wiced_result_t app_bt_set_advertisement_data(void) {
    wiced_result_t wiced_result = WICED_SUCCESS;
    wiced_result = wiced_bt_ble_set_raw_advertisement_data(3, cy_bt_adv_packet_data);

    return (wiced_result);
}

void ess_timer_callb(void *callback_arg, cyhal_lptimer_event_t event) {
    BaseType_t xHigherPriorityTaskWoken;
    xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(ess_task_handle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


void ess_task(void *pvParam) {
    static uint16_t lastVal = 0;
    lastVal = getTamperCount();

    while (true) {
        uint16_t tamperCount = getTamperCount();

        if (tamperCount != lastVal) {
            for (int i = 0; i < 2; i++) {
                cyhal_gpio_toggle(CYBSP_USER_LED);
                Cy_SysLib_Delay(50);
            }
        }
        lastVal = tamperCount;

        *(uint16_t*)app_bas_battery_information = tamperCount;

        if (IS_NOTIFIABLE(app_bt_conn_id, app_bas_battery_information_client_char_config[0])) {
            wiced_bt_gatt_status_t gatt_status = wiced_bt_gatt_server_send_indication(
                    app_bt_conn_id, HDLC_BAS_BATTERY_INFORMATION_VALUE, 
                    app_bas_battery_information_len, app_bas_battery_information, 
                    NULL);

            printf("Sent notification status 0x%x\n", gatt_status);
        } else {
            printf(app_bt_conn_id 
                    ? "Notifications off on host\n" 
                    : "Not connected\n");
        }

        // block until next timr cycle
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}


TaskHandle_t get_ess_handle() {
    return ess_task_handle;
}
