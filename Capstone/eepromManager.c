#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "cy_em_eeprom.h"
#include <stdint.h>
#include <stdbool.h>
#include "rtc.h"

/*******************************************************************************
 * Macros
 ******************************************************************************/
/* 
emEEPROM organised in the following way:
Tamper count - 1byte
Timestamps - up to 80bytes
 */
/* Location of reset counter in Em_EEPROM. */
#define TAMPER_COUNT_LOCATION (0u)
#define TAMPER_COUNT_SIZE (1u)

#define TIMESTAMP_LOCATION TAMPER_COUNT_SIZE
#define TIMESTAMP_SIZE (4u)
#define MAX_TIMESTAMP_COUNT (20)


/* Logical Size of Emulated EEPROM in bytes. */
#define LOGICAL_EEPROM_SIZE     (TIMESTAMP_LOCATION + MAX_TIMESTAMP_COUNT * TIMESTAMP_SIZE)
#define LOGICAL_EEPROM_START    (0u)

/* EEPROM Configuration details. All the sizes mentioned are in bytes.
 * For details on how to configure these values refer to cy_em_eeprom.h. The
 * library documentation is provided in Em EEPROM API Reference Manual. The user
 * access it from ModusToolbox IDE Quick Panel > Documentation> 
 * Cypress Em_EEPROM middleware API reference manual
 */
#define EEPROM_SIZE             (256u)
#define BLOCKING_WRITE          (1u)
#define REDUNDANT_COPY          (1u)
#define WEAR_LEVELLING_FACTOR   (2u)
#define SIMPLE_MODE             (0u)

/* Set the macro FLASH_REGION_TO_USE to either USER_FLASH or
 * EMULATED_EEPROM_FLASH to specify the region of the flash used for
 * emulated EEPROM.
 */
#define USER_FLASH              (0u)
#define EMULATED_EEPROM_FLASH   (1u)

#if CY_EM_EEPROM_SIZE
/* CY_EM_EEPROM_SIZE to determine whether the target has a dedicated EEPROM region or not */
#define FLASH_REGION_TO_USE     EMULATED_EEPROM_FLASH
#else
#define FLASH_REGION_TO_USE     USER_FLASH
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
uint16_t getTamperCount(void)
{
    uint8_t tamper_count;
    cy_en_em_eeprom_status_t eeprom_return_value;

    /* Read 15 bytes out of EEPROM memory. */
    eeprom_return_value = Cy_Em_EEPROM_Read(TAMPER_COUNT_LOCATION, &tamper_count,
                                          TAMPER_COUNT_SIZE, &Em_EEPROM_context);
    handle_eeprom_result(eeprom_return_value, "Emulated EEPROM Read failed \r\n");
    
    return tamper_count;
}

uint8_t increaseTamperCount(void) {
    /* Return status for EEPROM. */
    cy_en_em_eeprom_status_t eeprom_return_value;

    struct tm date_time;
    int timestamp;
    uint8_t tamper_count;
    
    /* Read 15 bytes out of EEPROM memory. */
    eeprom_return_value = Cy_Em_EEPROM_Read(TAMPER_COUNT_LOCATION, &tamper_count,
                                          TAMPER_COUNT_SIZE, &Em_EEPROM_context);
    handle_eeprom_result(eeprom_return_value, "Emulated EEPROM Read failed \r\n");
    

    tamper_count += 1;
    
    read_rtc(&date_time);
    timestamp = convert_rtc_to_int(&date_time);
    printf("\n%d\n", timestamp);
    
    eeprom_return_value = Cy_Em_EEPROM_Write(TIMESTAMP_LOCATION + TIMESTAMP_SIZE * (tamper_count-1),
                                            &timestamp,
                                            TIMESTAMP_SIZE,
                                            &Em_EEPROM_context);
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

void getTimestamps(int *timestamps) {
    uint8_t tamper_count;
    cy_en_em_eeprom_status_t eeprom_return_value;
    // int timestamps[MAX_TIMESTAMP_COUNT];

    tamper_count = getTamperCount();
    
    /* Read 15 bytes out of EEPROM memory. */
    eeprom_return_value = Cy_Em_EEPROM_Read(TIMESTAMP_LOCATION, timestamps,
                                          TIMESTAMP_SIZE*tamper_count, &Em_EEPROM_context);
    if (tamper_count > 0) {
        eeprom_return_value = Cy_Em_EEPROM_Read(TIMESTAMP_LOCATION, timestamps,
                                            TIMESTAMP_SIZE*tamper_count, &Em_EEPROM_context);;
        handle_eeprom_result(eeprom_return_value, "Emulated EEPROM timestamp Read failed \r\n");
    } else {
        memset(timestamps, 0, MAX_TIMESTAMP_COUNT * 4);
    }

    // for (int i = 0; i < MAX_TIMESTAMP_COUNT; i++) {}
    return;
    // printf("\n%d %d %d %d %d %d\n", thing, CY_EM_EEPROM_BAD_PARAM, CY_EM_EEPROM_BAD_CHECKSUM, CY_EM_EEPROM_BAD_DATA, CY_EM_EEPROM_WRITE_FAIL, CY_EM_EEPROM_REDUNDANT_COPY_USED);
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
                printf("%s",message);
            }

            while(1u);
        }
        else
        {
            printf("%s","Main copy is corrupted. Redundant copy in Emulated EEPROM is used \r\n");
        }

    }
}