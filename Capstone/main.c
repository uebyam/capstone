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

#include "main.h"
#include "eeprom.h"
#include "lpcomp.h"
#include "userbutton.h"

/* This is the temperature measurement interval which is same as configured in
 * the BT Configurator - The variable represents interval in milliseconds.
 */
#define POLL_TIMER_IN_MSEC (49999u)
#define POLL_TIMER_FREQ (10000)
/* Temperature Simulation Constants */
#define DEFAULT_TEMPERATURE (2500u)
#define MAX_TEMPERATURE_LIMIT (3000u)
#define MIN_TEMPERATURE_LIMIT (2000u)
#define DELTA_TEMPERATURE (100u)

/* Number of advertisment packet */
#define NUM_ADV_PACKETS (3u)

/* Absolute value of an integer. The absolute value is always positive. */
#ifndef ABS
#define ABS(N) ((N < 0) ? (-N) : (N))
#endif

/* Check if notification is enabled for a valid connection ID */
#define IS_NOTIFIABLE(conn_id, cccd) (((conn_id) != 0) ? (cccd) & GATT_CLIENT_CONFIG_INDICATION : 0)

/******************************************************************************
 *                                 TYPEDEFS
 ******************************************************************************/

/*******************************************************************************
 *        Variable Definitions
 *******************************************************************************/
/* Configuring Higher priority for the application */
volatile int uxTopUsedPriority;

/* Manages runtime configuration of Bluetooth stack */
extern const wiced_bt_cfg_settings_t wiced_bt_cfg_settings;

/* FreeRTOS variable to store handle of task created to update and send dummy
   values of temperature */
TaskHandle_t ess_task_handle;

/* Status variable for connection ID */
uint16_t app_bt_conn_id;

/* Dummy Room Temperature */
int16_t temperature = DEFAULT_TEMPERATURE;

/* Variable for 5 sec timer object */
static cyhal_timer_t ess_timer_obj;
/* Configure timer for 5 sec */
const cyhal_timer_cfg_t ess_timer_cfg =
    {
        .compare_value = 0,              /* Timer compare value, not used */
        .period = POLL_TIMER_IN_MSEC,    /* Defines the timer period */
        .direction = CYHAL_TIMER_DIR_UP, /* Timer counts up */
        .is_compare = false,             /* Don't use compare mode */
        .is_continuous = true,           /* Run timer indefinitely */
        .value = 0                       /* Initial value of counter */
};
/*******************************************************************************
 *        Function Prototypes
 *******************************************************************************/

/* Callback function for Bluetooth stack management type events */
static wiced_bt_dev_status_t
app_bt_management_callback(wiced_bt_management_evt_t event,
                           wiced_bt_management_evt_data_t *p_event_data);

/* This function sets the advertisement data */
static wiced_result_t app_bt_set_advertisement_data(void);

/* This function initializes the required BLE ESS & thermistor */
static void bt_app_init(void);

/* Task to send notifications with dummy temperature values */
void ess_task(void *pvParam);
/* HAL timer callback registered when timer reaches terminal count */
void ess_timer_callb(void *callback_arg, cyhal_timer_event_t event);

/* This function starts the advertisements */
static void app_start_advertisement(void);

uint16_t tamper_count = 0;

/******************************************************************************
 *                          Function Definitions
 ******************************************************************************/

/*
 *  Entry point to the application. Set device configuration and start BT
 *  stack initialization.  The actual application initialization will happen
 *  when stack reports that BT device is ready.
 */
int main(void) {
    if (Cy_SysPm_IoIsFrozen())
        Cy_SysPm_IoUnfreeze();

    bool didHib = false;
    if (Cy_SysLib_GetResetReason() & CY_SYSLIB_RESET_HIB_WAKEUP) {
        Cy_SysLib_ClearResetReason();
        didHib = true;
    }

    uxTopUsedPriority = configMAX_PRIORITIES - 1;
    wiced_result_t wiced_result;
    BaseType_t rtos_result;

    /* Initialize and Verify the BSP initialization */
    CY_ASSERT(CY_RSLT_SUCCESS == cybsp_init());

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX,
                        CYBSP_DEBUG_UART_RX,
                        CY_RETARGET_IO_BAUDRATE);

    /* Initialising the HCI UART for Host contol */
    cybt_platform_config_init(&cybsp_bt_platform_cfg);

    /* Debug logs on UART port */

    printf("****** Environmental Sensing Service ******\n");

    init_lpcomp();
    init_userbutton();
    initEEPROM();

    /* Register call back and configuration with stack */
    wiced_result = wiced_bt_stack_init(app_bt_management_callback,
                                       &wiced_bt_cfg_settings);

    /* Check if stack initialization was successful */
    if (WICED_BT_SUCCESS == wiced_result)
    {
        printf("Bluetooth Stack Initialization Successful \n");
    }
    else
    {
        printf("Bluetooth Stack Initialization failed!!\n");
    }

    rtos_result = xTaskCreate(ess_task, "ESS Task", (configMINIMAL_STACK_SIZE * 4),
                              NULL, (configMAX_PRIORITIES - 3), &ess_task_handle);
    if (pdPASS == rtos_result)
    {
        printf("ESS task created successfully\n");
    }
    else
    {
        printf("ESS task creation failed\n");
    }

    tamper_count = getTamperCount();

    printf("Current tamper count: %u\n", tamper_count);


    vTaskStartScheduler();

    /* Should never get here */
    CY_ASSERT(0);
}

/*
 * Function Name: app_bt_management_callback()
 *
 *@brief
 *  This is a Bluetooth stack event handler function to receive management events
 *  from the Bluetooth LE stack and process as per the application.
 *
 * @param wiced_bt_management_evt_t  Bluetooth LE event code of one byte length
 * @param wiced_bt_management_evt_data_t  Pointer to Bluetooth LE management event
 *                                        structures
 *
 * @return wiced_result_t Error code from WICED_RESULT_LIST or BT_RESULT_LIST
 *
 */
static wiced_result_t
app_bt_management_callback(wiced_bt_management_evt_t event,
                           wiced_bt_management_evt_data_t *p_event_data)
{
    wiced_bt_dev_status_t status = WICED_ERROR;

    switch (event)
    {

    case BTM_ENABLED_EVT:
        printf("\nThis application implements Bluetooth LE Environmental Sensing\n"
               "Service and sends dummy temperature values in Celsius\n"
               "every %d milliseconds over Bluetooth\n",
               (POLL_TIMER_IN_MSEC));

        printf("Discover this device with the name:%s\n", app_gap_device_name);

        print_local_bd_address();

        printf("\n");
        printf("Bluetooth Management Event: \t");
        printf("%s", get_btm_event_name(event));
        printf("\n");

        /* Perform application-specific initialization */
        bt_app_init();
        break;

    case BTM_DISABLED_EVT:
        /* Bluetooth Controller and Host Stack Disabled */
        printf("\n");
        printf("Bluetooth Management Event: \t");
        printf("%s", get_btm_event_name(event));
        printf("\n");
        printf("Bluetooth Disabled\n");
        break;

    case BTM_BLE_ADVERT_STATE_CHANGED_EVT:
        /* Advertisement State Changed */
        printf("\n");
        printf("Bluetooth Management Event: \t");
        printf("%s", get_btm_event_name(event));
        printf("\n");
        printf("\n");
        printf("Advertisement state changed to ");
        printf("%s", get_btm_advert_mode_name(p_event_data->ble_advert_state_changed));
        printf("\n");
        break;

    default:
        printf("\nUnhandled Bluetooth Management Event: %d %s\n",
               event,
               get_btm_event_name(event));
        break;
    }

    return (status);
}

/*
 Function name:
 bt_app_init

 Function Description:
 @brief    This function is executed if BTM_ENABLED_EVT event occurs in
           Bluetooth management callback.

 @param    void

 @return    void
 */
static void bt_app_init(void)
{
    wiced_bt_gatt_status_t gatt_status = WICED_BT_GATT_ERROR;
    cy_rslt_t rslt;

    /* Register with stack to receive GATT callback */
    gatt_status = wiced_bt_gatt_register(app_bt_gatt_event_callback);
    printf("\n gatt_register status:\t%s\n", get_gatt_status_name(gatt_status));

    /* Initialize the User LED */
    cyhal_gpio_init(CONNECTION_LED,
                    CYHAL_GPIO_DIR_OUTPUT,
                    CYHAL_GPIO_DRIVE_STRONG,
                    CYBSP_LED_STATE_OFF);

    /* Initialize GATT Database */
    gatt_status = wiced_bt_gatt_db_init(gatt_database, gatt_database_len, NULL);
    if (WICED_BT_GATT_SUCCESS != gatt_status)
        printf("\n GATT DB Initialization not successful err 0x%x\n", gatt_status);

    /* Start Bluetooth LE advertisements */
    app_start_advertisement();
}

/**
 * @brief This function starts the Blueooth LE advertisements and describes
 *        the pairing support
 */
static void app_start_advertisement(void)
{
    wiced_result_t wiced_status;

    /* Set Advertisement Data */
    wiced_status = app_bt_set_advertisement_data();
    if (WICED_SUCCESS != wiced_status)
        printf("Raw advertisement failed err 0x%x\n", wiced_status);

    /* Do not allow peer to pair */
    wiced_bt_set_pairable_mode(WICED_FALSE, FALSE);

    /* Start Undirected LE Advertisements on device startup. */
    wiced_status = wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH,
                                                 BLE_ADDR_PUBLIC,
                                                 NULL);

    if (WICED_SUCCESS != wiced_status) {
        printf("Starting undirected Bluetooth LE advertisements"
               "Failed err 0x%x\n",
               wiced_status);
    }
}

/*
 Function Name:
 app_bt_set_advertisement_data

 Function Description:
 @brief  Set Advertisement Data

 @param void

 @return wiced_result_t WICED_SUCCESS or WICED_failure
 */
static wiced_result_t app_bt_set_advertisement_data(void)
{

    wiced_result_t wiced_result = WICED_SUCCESS;
    wiced_result = wiced_bt_ble_set_raw_advertisement_data(NUM_ADV_PACKETS,
                                                           cy_bt_adv_packet_data);

    return (wiced_result);
}

/*
 Function name:
 ess_timer_callb

 Function Description:
 @brief  This callback function is invoked on timeout of 5 seconds timer.

 @param  void*: unused
 @param cyhal_timer_event_t: unused

 @return void
 */
void ess_timer_callb(void *callback_arg, cyhal_timer_event_t event)
{
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
                    ? "Notifications off on host\r\n" 
                    : "Not connected\r\n");
        }

        // block until next timr cycle
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}


TaskHandle_t get_ess_handle() {
    return ess_task_handle;
}
