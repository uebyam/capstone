#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif
#include "wiced_bt_gatt.h"
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
#include "bt_utils.h"
#include "wiced_bt_ble.h"
#include "wiced_bt_uuid.h"
#include "wiced_memory.h"
#include "wiced_bt_stack.h"
#include "cycfg_bt_settings.h"
#include "cycfg_gap.h"
#include "cybsp_bt_config.h"
#include "cy_em_eeprom.h"

#include "ansi.h"
#include "uart.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "main.h"
bool global_bluetooth_started = false;
bool global_bluetooth_enabled = false;
uint8_t global_bt_page = 0;

#include "eepromManager.h"
#include "lpcomp.h"
#include "userbutton.h"
#include "rtc.h"

#ifndef ABS
#define ABS(N) ((N < 0) ? (-N) : (N))
#endif

#define IS_NOTIFIABLE(conn_id, cccd) (((conn_id) != 0) ? (cccd) & GATT_CLIENT_CONFIG_NOTIFICATION : 0)

volatile int uxTopUsedPriority;
extern const wiced_bt_cfg_settings_t wiced_bt_cfg_settings;

TaskHandle_t ess_task_handle;
TimerHandle_t watchdog_timer_handle;

uint16_t app_bt_conn_id;

static wiced_bt_dev_status_t app_bt_management_callback(wiced_bt_management_evt_t event, wiced_bt_management_evt_data_t *p_event_data);
static wiced_result_t app_bt_set_advertisement_data(void);
static void bt_app_init(void);
static void app_start_advertisement(void);

void ess_task(void *pvParam);

void watchdog_reset_callback(TimerHandle_t timer);

int main(void) {
    CY_ASSERT(CY_RSLT_SUCCESS == cybsp_init());
    __enable_irq();

    Cy_SysClk_PllDisable(1);
    Cy_SysClk_FllDisable();
    Cy_SysClk_FllConfigure(8000000, 50000000, CY_SYSCLK_FLLPLL_OUTPUT_OUTPUT);
    Cy_SysClk_FllEnable(1000);
    Cy_SysPm_SwitchToSimoBuck();
    SystemCoreClockUpdate();

    Cy_WDT_Unlock();
    Cy_WDT_Init();
    Cy_WDT_ClearInterrupt();
    Cy_WDT_Lock();

    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);

    LOG_FATAL_NOFMT("\n");

    uint32_t resetReason = Cy_SysLib_GetResetReason();
    if (resetReason & CY_SYSLIB_RESET_HWWDT) {
        LOG_WARN("System reset due to WDT (system hang)\n");
    }
    if (resetReason & CY_SYSLIB_RESET_ACT_FAULT) {
        LOG_WARN("System reset due to ACT FAULT\n");
    }
    if (resetReason & CY_SYSLIB_RESET_DPSLP_FAULT) {
        LOG_WARN("System reset due to DPSLP FAULT\n");
    }
    if (resetReason & CY_SYSLIB_RESET_SOFT) {
        LOG_INFO("System reset due to soft reset\n");
    }
    if (resetReason & CY_SYSLIB_RESET_HIB_WAKEUP) {
        LOG_DEBUG("System reset due to hibernation wakeup\n");
    }
    if (resetReason & CY_SYSLIB_RESET_SWWDT1) {
        LOG_INFO("System reset due to MCWDT 1\n");
    }
    if (resetReason & CY_SYSLIB_RESET_SWWDT2) {
        LOG_INFO("System reset due to MCWDT 1\n");
    }
    if (resetReason & CY_SYSLIB_RESET_SWWDT3) {
        LOG_INFO("System reset due to MCWDT 1\n");
    }

    Cy_SysLib_ClearResetReason();

    init_userbutton();
    init_lpcomp(1);
    initEEPROM();
    init_rtc(1);

    if (init_uart()) {
        LOG_ERR("UART initialisation failed\n");
    } else {
        LOG_DEBUG("Initialised UART\n");
    }

    Cy_WDT_Unlock();
    Cy_WDT_Enable();
    Cy_WDT_Lock();

    uxTopUsedPriority = configMAX_PRIORITIES - 1;
    BaseType_t rtos_result;

    cyhal_gpio_init(CONNECTION_LED, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);

    LOG_INFO("****** Tamper Sensing Service ******\n");

    rtos_result = xTaskCreate(ess_task, "ESS Task", (configMINIMAL_STACK_SIZE * 4),
                              NULL, (configMAX_PRIORITIES - 3), &ess_task_handle);

    if (pdPASS == rtos_result) {
        LOG_DEBUG("ESS task created successfully\n");
    } else {
        LOG_ERR("ESS task creation failed\n");
    }

    watchdog_timer_handle = xTimerCreate("watchdogres", pdMS_TO_TICKS(500), pdTRUE, 0, watchdog_reset_callback);
    if (watchdog_timer_handle) {
        xTimerStart(watchdog_timer_handle, 0);
    } else {
        LOG_DEBUG("Watchdog timer reset created\n");
    }

#if (LOGLEVEL > 4)
    uint16_t tamper_count = getTamperCount();

    LOG_DEBUG("Current tamper count: %u\n", tamper_count);
#endif

    start_bt();
    vTaskStartScheduler();

    // Should never get here
    LOG_ERR("Program termination\r\n");
}

void start_bt(void) {
    global_bluetooth_started = true;
    wiced_result_t wiced_result;

    cybt_platform_config_init(&cybsp_bt_platform_cfg);
    wiced_result = wiced_bt_stack_init(app_bt_management_callback, &wiced_bt_cfg_settings);

    if (WICED_BT_SUCCESS == wiced_result) {
        LOG_DEBUG("Bluetooth Stack Initialization Successful\n");
    } else {
        LOG_FATAL("Bluetooth Stack Initialization failed!!\n");
        global_bluetooth_started = false;
    }
}

static wiced_result_t app_bt_management_callback(wiced_bt_management_evt_t event, wiced_bt_management_evt_data_t *p_event_data) {
    switch (event) {
        case BTM_ENABLED_EVT:
            LOG_DEBUG(" -- Bluetooth event: %s\n", get_btm_event_name(event));
            LOG_INFO("Bluetooth enabled\n");
            LOG_INFO("Device name: %s\n", app_gap_device_name);

            LOG_DEBUG("");
            print_local_bd_address();

            bt_app_init();
            global_bluetooth_enabled = true;
            break;

        case BTM_DISABLED_EVT:
            LOG_WARN("Bluetooth disabled\n");
            break;

        case BTM_BLE_ADVERT_STATE_CHANGED_EVT:
            LOG_DEBUG("Advertisement state changed to %s\n",
                    get_ble_advert_mode_name(p_event_data->ble_advert_state_changed));
            break;

        default:
            LOG_DEBUG("Unhandled bluetooth event: %s (%d)\n", get_btm_event_name(event), event);
            break;
    }

    return WICED_SUCCESS;
}

static void bt_app_init(void) {
    wiced_bt_gatt_status_t gatt_status = WICED_BT_GATT_ERROR;

    gatt_status = wiced_bt_gatt_register(app_bt_gatt_event_callback);
    if (gatt_status) {
        LOG_ERR("GATT registration failed with %s\n", get_gatt_status_name(gatt_status));
    } else {
        LOG_DEBUG("GATT registration returned %s\n", get_gatt_status_name(gatt_status));
    }

    gatt_status = wiced_bt_gatt_db_init(gatt_database, gatt_database_len, NULL);
    if (gatt_status) {
        LOG_ERR("GATT database initialisation failed with %s\n", get_gatt_status_name(gatt_status));
    } else {
        LOG_DEBUG("GATT database initialisation returned %s\n", get_gatt_status_name(gatt_status));
    }

    app_start_advertisement();
}

void global_start_advertisement(void) {
    app_start_advertisement();
}

static void app_start_advertisement(void) {
    wiced_result_t wiced_status;

    wiced_status = app_bt_set_advertisement_data();
    if (WICED_SUCCESS != wiced_status) {
        LOG_ERR("Setting raw advertisement data failed with %s (0x%x)\n", get_wiced_result_name(wiced_status), wiced_status);
    } else {
        LOG_DEBUG("Setting raw advertisement data succeeded\n");
    }

    wiced_bt_set_pairable_mode(WICED_FALSE, FALSE);

    wiced_status = wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH, BLE_ADDR_PUBLIC, NULL);
    if (WICED_SUCCESS != wiced_status) {
        LOG_ERR("Starting undirected Bluetooth LE advertisements failed err 0x%x\n", wiced_status);
    }
}

static wiced_result_t app_bt_set_advertisement_data(void) {
    wiced_result_t wiced_result = WICED_SUCCESS;
    wiced_result = wiced_bt_ble_set_raw_advertisement_data(3, cy_bt_adv_packet_data);

    return (wiced_result);
}

void ess_task(void *pvParam) {
    /*                          *                           
     *      those who know      *
     *                          */
    int _timestamps[GLOBAL_BT_PAGE_SIZE + 1] = {};
    int *timestamps = &(_timestamps[1]);
    uint8_t _tamper_types[GLOBAL_BT_PAGE_SIZE + 1] = {};
    uint8_t *tamper_types = &(_tamper_types[1]);
    while (true) {
        uint16_t tamperCount = getTamperCount();
        *(uint16_t*)app_tamper_information_tamper_count = tamperCount;
        
        memset(timestamps, 0, GLOBAL_BT_PAGE_SIZE);
        LOG_DEBUG("Reading %u timestamp(s) at offset %u\n", GLOBAL_BT_PAGE_SIZE, GLOBAL_BT_PAGE_SIZE * global_bt_page);
        getTimestamps(timestamps, tamper_types, GLOBAL_BT_PAGE_SIZE * global_bt_page, GLOBAL_BT_PAGE_SIZE);

        if (tamperCount > GLOBAL_BT_PAGE_SIZE) tamperCount = GLOBAL_BT_PAGE_SIZE;

        memcpy(app_tamper_information_timestamps, _timestamps, sizeof _timestamps);
        memcpy(app_tamper_information_tamper_count, _tamper_types, sizeof _tamper_types);

        if (global_bluetooth_started) {
            LOG_DEBUG("Attempting to send tamper count notifications/indications...\n");
            wiced_bt_gatt_status_t gatt_status = WICED_BT_GATT_ERROR;
            switch (app_tamper_information_tamper_count_client_char_config[0]) {
                case 0:
                    LOG_DEBUG(app_bt_conn_id
                            ? "Notifications/indications for tamper count off on host\n"
                            : "Not connected\n");
                    break;
                case 3:
                case 1:
                    gatt_status = wiced_bt_gatt_server_send_indication(
                            app_bt_conn_id, HDLC_TAMPER_INFORMATION_TAMPER_COUNT_VALUE,
                            app_tamper_information_tamper_count_len, app_tamper_information_tamper_count,
                            NULL);
                    LOG_DEBUG("Sending tamper count notification returned %s\n", get_gatt_status_name(gatt_status));
                    break;
                case 2:
                    gatt_status = wiced_bt_gatt_server_send_notification(
                            app_bt_conn_id, HDLC_TAMPER_INFORMATION_TAMPER_COUNT_VALUE, 
                            app_tamper_information_tamper_count_len, app_tamper_information_tamper_count,
                            NULL);
                    LOG_DEBUG("Sending tamper count indication returned %s\n", get_gatt_status_name(gatt_status));
                    break;
            }

            LOG_DEBUG("Attempting to send timestamp notifications/indications...\n");
            switch (app_tamper_information_timestamps_client_char_config[0]) {
                case 0:
                    LOG_DEBUG(app_bt_conn_id
                            ? "Notifications/indications for timestamps off on host\n"
                            : "Not connected\n");
                    break;
                case 3:
                case 1:
                    gatt_status = wiced_bt_gatt_server_send_indication(
                            app_bt_conn_id, HDLC_TAMPER_INFORMATION_TIMESTAMPS_VALUE,
                            app_tamper_information_timestamps_len, app_tamper_information_timestamps,
                            NULL);
                    LOG_DEBUG("Sending timestamps notification returned %s\n", get_gatt_status_name(gatt_status));
                    break;
                case 2:
                    gatt_status = wiced_bt_gatt_server_send_notification(
                            app_bt_conn_id, HDLC_TAMPER_INFORMATION_TIMESTAMPS_VALUE, 
                            app_tamper_information_timestamps_len, app_tamper_information_timestamps,
                            NULL);
                    LOG_DEBUG("Sending timestamps indication returned %s\n", get_gatt_status_name(gatt_status));
                    break;
            }

            LOG_DEBUG("Attempting to send tamper types notifications/indications...\n");
            switch (app_tamper_information_tamper_type_client_char_config[0]) {
                case 0:
                    LOG_DEBUG(app_bt_conn_id
                            ? "Notifications/indications for types off on host\n"
                            : "Not connected\n");
                    break;
                case 3:
                case 1:
                    gatt_status = wiced_bt_gatt_server_send_indication(
                            app_bt_conn_id, HDLC_TAMPER_INFORMATION_TAMPER_TYPE_VALUE,
                            app_tamper_information_tamper_type_len, app_tamper_information_tamper_type,
                            NULL);
                    LOG_DEBUG("Sending types notification returned %s\n", get_gatt_status_name(gatt_status));
                    break;
                case 2:
                    gatt_status = wiced_bt_gatt_server_send_notification(
                            app_bt_conn_id, HDLC_TAMPER_INFORMATION_TAMPER_TYPE_VALUE, 
                            app_tamper_information_tamper_type_len, app_tamper_information_tamper_type,
                            NULL);
                    LOG_DEBUG("Sending types indication returned %s\n", get_gatt_status_name(gatt_status));
                    break;
            }
        } else {
            LOG_DEBUG("ESS task called when bluetooth was not started; no action taken\n");
        }

        // block until next timr cycle
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}


TaskHandle_t get_ess_handle() {
    return ess_task_handle;
}

void watchdog_reset_callback(TimerHandle_t timer) {
    Cy_WDT_ClearWatchdog();
}
