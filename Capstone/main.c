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

uint16_t app_bt_conn_id;

static wiced_bt_dev_status_t app_bt_management_callback(wiced_bt_management_evt_t event, wiced_bt_management_evt_data_t *p_event_data);
static wiced_result_t app_bt_set_advertisement_data(void);
static void bt_app_init(void);
static void app_start_advertisement(void);

void ess_task(void *pvParam);

int main(void) {
    CY_ASSERT(CY_RSLT_SUCCESS == cybsp_init());
    __enable_irq();

    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);

    init_userbutton();
    init_lpcomp(1);
    initEEPROM();
    init_rtc(0);

    if (init_uart()) {
        LOG_ERR("UART initialisation failed\n");
    } else {
        LOG_DEBUG("Initialised UART\n");
    }

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

    uint16_t tamper_count = getTamperCount();

    LOG_DEBUG("Current tamper count: %u\n", tamper_count);

    // Bluetooth application initialisation is started from user button
    vTaskStartScheduler();

    // Should never get here
    LOG_ERR("Program termination\r\n");
}

void start_bt() {
    global_bluetooth_started = true;
    wiced_result_t wiced_result;

    cybt_platform_config_init(&cybsp_bt_platform_cfg);
    wiced_result = wiced_bt_stack_init(app_bt_management_callback, &wiced_bt_cfg_settings);

    if (WICED_BT_SUCCESS == wiced_result) {
        LOG_INFO("Bluetooth Stack Initialization Successful\n");
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
            break;

        case BTM_DISABLED_EVT:
            LOG_WARN("Bluetooth disabled\n");
            break;

        case BTM_BLE_ADVERT_STATE_CHANGED_EVT:
            LOG_INFO("Advertisement state changed to %s\n",
                    get_ble_advert_mode_name(p_event_data->ble_advert_state_changed));
            break;

        default:
            LOG_WARN("Unhandled bluetooth event: %s (%d)\n", get_btm_event_name(event), event);
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
    int timestamps[MAX_TIMESTAMP_COUNT];
    while (true) {
        uint16_t tamperCount = getTamperCount();
        *(uint16_t*)app_tamper_information_tamper_count = tamperCount;
        
        getTimestamps(timestamps);
        LOG_DEBUG("Timestamp information:\n");
        LOG_DEBUG("");
        memcpy(app_tamper_information_timestamps, timestamps, tamperCount*TIMESTAMP_SIZE);
        for (int i = 0; i < tamperCount; i++) {
            LOG_DEBUG_NOFMT("%d ", timestamps[i]);
        }
        LOG_DEBUG_NOFMT("\n");
        LOG_DEBUG("");
        for (int i = 0; i < tamperCount; i++) {
            int t;
            memcpy(&t, &app_tamper_information_timestamps[i*4], 4);
            LOG_DEBUG_NOFMT("%d ", t);
        }
        LOG_DEBUG_NOFMT("\n");

        if (global_bluetooth_started) {
            switch (app_tamper_information_tamper_count_client_char_config[0]) {
                case 0:
                    LOG_INFO(app_bt_conn_id
                            ? "Notifications/indications for tamper count off on host\n"
                            : "Not connected\n");
                    break;
                case 3:
                case 1:
                    wiced_bt_gatt_server_send_indication(
                            app_bt_conn_id, HDLC_TAMPER_INFORMATION_TAMPER_COUNT_VALUE,
                            app_tamper_information_tamper_count_len, app_tamper_information_tamper_count,
                            NULL);
                    break;
                case 2:
                    wiced_bt_gatt_server_send_notification(
                            app_bt_conn_id, HDLC_TAMPER_INFORMATION_TAMPER_COUNT_VALUE, 
                            app_tamper_information_tamper_count_len, app_tamper_information_tamper_count,
                            NULL);
                    break;
            }

            switch (app_tamper_information_timestamps_client_char_config[0]) {
                case 0:
                    LOG_INFO(app_bt_conn_id
                            ? "Notifications/indications for timestamps off on host\n"
                            : "Not connected\n");
                    break;
                case 3:
                case 1:
                    wiced_bt_gatt_server_send_indication(
                            app_bt_conn_id, HDLC_TAMPER_INFORMATION_TIMESTAMPS_VALUE,
                            app_tamper_information_timestamps_len, app_tamper_information_timestamps,
                            NULL);
                    break;
                case 2:
                    wiced_bt_gatt_server_send_notification(
                            app_bt_conn_id, HDLC_TAMPER_INFORMATION_TAMPER_COUNT_VALUE, 
                            app_tamper_information_timestamps_len, app_tamper_information_timestamps,
                            NULL);
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
