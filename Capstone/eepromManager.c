#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "cy_em_eeprom.h"
#include <stdint.h>
#include <stdbool.h>
#include "rtc.h"
#include "ansi.h"
#include "eepromManager.h"
#include "main.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

/*******************************************************************************
 * Function Prototypes
 ******************************************************************************/
void handle_eeprom_result(uint32_t status, char *message);

/*******************************************************************************
 * Global variables
 ******************************************************************************/
/* EEPROM configuration and context structure. */
cy_stc_eeprom_config_t Em_EEPROM_config =
{
        .eepromSize = EEPROM_SIZE,
        .blockingWrite = BLOCKING_WRITE,
        .redundantCopy = REDUNDANT_COPY,
        .wearLevelingFactor = WEAR_LEVELLING_FACTOR,
};

cy_stc_eeprom_context_t Em_EEPROM_context;

#if (EMULATED_EEPROM_FLASH == FLASH_REGION_TO_USE)
CY_SECTION(".cy_em_eeprom")
#endif /* #if(FLASH_REGION_TO_USE) */
CY_ALIGN(CY_EM_EEPROM_FLASH_SIZEOF_ROW)

#if (defined(CY_DEVICE_SECURE) && (USER_FLASH == FLASH_REGION_TO_USE ))
/* When CY8CKIT-064B0S2-4343W is selected as the target and EEPROM array is
 * stored in user flash, the EEPROM array is placed in a fixed location in
 * memory. The adddress of the fixed location can be arrived at by determining
 * the amount of flash consumed by the application. In this case, the example
 * consumes approximately 104000 bytes for the above target using GCC_ARM 
 * compiler and Debug configuration. The start address specified in the linker
 * script is 0x10000000, providing an offset of approximately 32 KB, the EEPROM
 * array is placed at 0x10021000 in this example. Note that changing the
 * compiler and the build configuration will change the amount of flash
 * consumed. As a resut, you will need to modify the value accordingly. Among
 * the supported compilers and build configurations, the amount of flash
 * consumed is highest for GCC_ARM compiler and Debug build configuration.
 */
#define APP_DEFINED_EM_EEPROM_LOCATION_IN_FLASH  (0x10021000)
#else
/* EEPROM storage in user flash or emulated EEPROM flash. */
const uint8_t eeprom_storage[CY_EM_EEPROM_GET_PHYSICAL_SIZE(EEPROM_SIZE, SIMPLE_MODE, WEAR_LEVELLING_FACTOR, REDUNDANT_COPY)] = {0u};

#endif /* #if (defined(CY_DEVICE_SECURE)) */

/* RAM arrays for holding EEPROM read and write data respectively. */
uint8_t eeprom_read_array[LOGICAL_EEPROM_SIZE];

void initEEPROM() {
    cy_en_em_eeprom_status_t eeprom_return_value;

#if (defined(CY_DEVICE_SECURE) && (USER_FLASH == FLASH_REGION_TO_USE ))
    Em_EEPROM_config.userFlashStartAddr = (uint32_t) APP_DEFINED_EM_EEPROM_LOCATION_IN_FLASH;
#else
    Em_EEPROM_config.userFlashStartAddr = (uint32_t) eeprom_storage;
#endif

    eeprom_return_value = Cy_Em_EEPROM_Init(&Em_EEPROM_config, &Em_EEPROM_context);
    handle_eeprom_result(eeprom_return_value, "Emulated EEPROM Initialization Error \r\n");
}

/*******************************************************************************
* Function Name: getTamperCount
********************************************************************************
*
* Summary:
* Reads from EEPROM and returns number of tampers as int
* If data isn't already initialised in EEPROM, creates an array of text to indicate presence of data
* If data is already intialised, increments by 1 on each execution
*
* Return: int
*
*******************************************************************************/
uint8_t getTamperCount(void)
{
    uint8_t tamper_count;
    cy_en_em_eeprom_status_t eeprom_return_value;

    /* Read 15 bytes out of EEPROM memory. */
    eeprom_return_value = Cy_Em_EEPROM_Read(TAMPER_COUNT_LOCATION, &tamper_count,
                                          TAMPER_COUNT_SIZE, &Em_EEPROM_context);
    handle_eeprom_result(eeprom_return_value, "Emulated EEPROM Read failed \r\n");
    
    return tamper_count;
}

uint8_t increaseTamperCount(eeprom_tamper_type_t tamper_type) {
    /* Return status for EEPROM. */
    cy_en_em_eeprom_status_t eeprom_return_value;

    struct tm date_time;
    int timestamp;
    uint8_t tamper_count;
    
    /* Read 15 bytes out of EEPROM memory. */
    eeprom_return_value = Cy_Em_EEPROM_Read(TAMPER_COUNT_LOCATION, &tamper_count,
                                          TAMPER_COUNT_SIZE, &Em_EEPROM_context);
    handle_eeprom_result(eeprom_return_value, "Emulated EEPROM Read failed \r\n");
    
    read_rtc(&date_time);
    LOG_DEBUG("RTC Read result: %02u:%02u:%02u %02u/%02u/%02u\n",
            date_time.tm_hour, date_time.tm_min, date_time.tm_sec,
            date_time.tm_mday, date_time.tm_mon, date_time.tm_year);
    timestamp = convert_rtc_to_int(&date_time);
    LOG_DEBUG("Converted timestamp: %u\n", timestamp);
    
    // timestamp
    eeprom_return_value = Cy_Em_EEPROM_Write(TIMESTAMP_LOCATION + TIMESTAMP_SIZE * (tamper_count % MAX_TIMESTAMP_COUNT),
                                            &timestamp,
                                            TIMESTAMP_SIZE,
                                            &Em_EEPROM_context);
    handle_eeprom_result(eeprom_return_value, "Emulated EEPROM Write failed \r\n");

    // tamper type
    eeprom_return_value = Cy_Em_EEPROM_Write(TAMPER_TYPE_LOCATION + TAMPER_TYPE_SIZE * (tamper_count % MAX_TIMESTAMP_COUNT),
                                            &tamper_type,
                                            TAMPER_TYPE_SIZE,
                                            &Em_EEPROM_context);
    handle_eeprom_result(eeprom_return_value, "Emulated EEPROM Write failed \r\n");


    tamper_count += 1;

    handle_eeprom_result(eeprom_return_value, "Emulated EEPROM Write failed \r\n");
    
    /* Only update the two count values in the EEPROM. */
    eeprom_return_value = Cy_Em_EEPROM_Write(TAMPER_COUNT_LOCATION,
                                            &tamper_count,
                                            TAMPER_COUNT_SIZE,
                                            &Em_EEPROM_context);
    handle_eeprom_result(eeprom_return_value, "Emulated EEPROM Write failed \r\n");

    return tamper_count;
}

void setTamperCount(uint8_t val) {
    cy_en_em_eeprom_status_t eeprom_return_value;
    eeprom_return_value = Cy_Em_EEPROM_Write(TAMPER_COUNT_LOCATION,
                                             &val,
                                             sizeof val,
                                             &Em_EEPROM_context);

    handle_eeprom_result(eeprom_return_value, "Emulated EEPROM Write failed \r\n");
}

void reset_tampers() {
	setTamperCount(0);
	char tempbuf1[BT_PAGE_SIZE] = {};
	uint32_t timebuf[BT_PAGE_SIZE] = {};
	Cy_Em_EEPROM_Write(TAMPER_TYPE_LOCATION, tempbuf1, sizeof tempbuf1, &Em_EEPROM_context);
	Cy_Em_EEPROM_Write(TIMESTAMP_LOCATION, timebuf, sizeof timebuf, &Em_EEPROM_context);
}


void getTimestamps(int *timestamps, uint8_t *tamper_types, size_t offset, size_t count) {
    uint8_t tamper_count;
    cy_en_em_eeprom_status_t eeprom_return_value;

	if (offset >= MAX_TIMESTAMP_COUNT) return;

	if ((count + offset) > MAX_TIMESTAMP_COUNT)
		count = MAX_TIMESTAMP_COUNT - offset;
    
    /* Read 15 bytes out of EEPROM memory. */
    if (count > 0) {
        if (timestamps) {
            eeprom_return_value = Cy_Em_EEPROM_Read(
                    TIMESTAMP_LOCATION + (offset * TIMESTAMP_SIZE),
                    timestamps,
                    TIMESTAMP_SIZE * count,
                    &Em_EEPROM_context);

            handle_eeprom_result(eeprom_return_value, "Emulated EEPROM timestamp Read failed \r\n");
		}

		if (tamper_types) {
            eeprom_return_value = Cy_Em_EEPROM_Read(
                    TAMPER_TYPE_LOCATION + (offset * TAMPER_TYPE_SIZE),
                    tamper_types,
                    TAMPER_TYPE_SIZE * count,
                    &Em_EEPROM_context);

            handle_eeprom_result(eeprom_return_value, "Emulated EEPROM timestamp Read failed \r\n");
        }
    }

    return;
}

/*******************************************************************************
* Function Name: handle_eeprom_result
********************************************************************************
*
* Summary:
* This function processes unrecoverable errors such as any component
* initialization errors etc. In case of such error the system will
* stay in the infinite loop of this function.
*
* Parameters:
* uint32_t status: contains the status.
* char* message: contains the message that is printed to the serial terminal.
*
* Note: If error occurs interrupts are disabled.
*
*******************************************************************************/
void handle_eeprom_result(uint32_t status, char *message)
{

    if(CY_EM_EEPROM_SUCCESS != status)
    {
        if(CY_EM_EEPROM_REDUNDANT_COPY_USED != status)
        {
            // cyhal_gpio_write((cyhal_gpio_t) CYBSP_USER_LED, false);
            __disable_irq();

            if(NULL != message)
            {
                LOG_DEBUG("%s",message);
            }

            while(1u);
        }
        else
        {
            LOG_DEBUG("%s","Main copy is corrupted. Redundant copy in Emulated EEPROM is used \r\n");
        }

    }
}
