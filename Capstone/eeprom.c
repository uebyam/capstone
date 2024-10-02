#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "cy_em_eeprom.h"


/*******************************************************************************
 * Macros
 ******************************************************************************/
/* Logical Size of Emulated EEPROM in bytes. */
#define LOGICAL_EEPROM_SIZE     (15u)
#define LOGICAL_EEPROM_START    (0u)


/* Location of reset counter in Em_EEPROM. */
#define RESET_COUNT_LOCATION    (13u)
/* Size of reset counter in bytes. */
#define COUNT_SIZE        (2u)

/* ASCII "9" */
#define ASCII_NINE              (0x39)

/* ASCII "0" */
#define ASCII_ZERO              (0x30)

/* ASCII "P" */
#define ASCII_INTIAL                 (0x54)

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

// #define GPIO_LOW                (0u)

/*******************************************************************************
 * Function Prototypes
 ******************************************************************************/
void handle_error(uint32_t status, char *message);


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
uint8_t eeprom_write_array[RESET_COUNT_LOCATION] = { 0x54, 0x61, 0x6D, 0x70, 0x65, 0x72, 0x43, 0x6F, 0x75, 0x6E, 0x74, 0x20, 0x23};;
                                                /* P, o, w, e, r, , C, y, c, l, e, #, ' ' */
uint16_t tampers_count = 0;

void initEEPROM() {
    printf("EmEEPROM demo \r\n");

    cy_en_em_eeprom_status_t eeprom_return_value;

#if (defined(CY_DEVICE_SECURE) && (USER_FLASH == FLASH_REGION_TO_USE ))
    Em_EEPROM_config.userFlashStartAddr = (uint32_t) APP_DEFINED_EM_EEPROM_LOCATION_IN_FLASH;
#else
    Em_EEPROM_config.userFlashStartAddr = (uint32_t) eeprom_storage;
#endif

    eeprom_return_value = Cy_Em_EEPROM_Init(&Em_EEPROM_config, &Em_EEPROM_context);
    handle_error(eeprom_return_value, "Emulated EEPROM Initialization Error \r\n");
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
    int count;
    /* Return status for EEPROM. */
    cy_en_em_eeprom_status_t eeprom_return_value;

    /* Read 15 bytes out of EEPROM memory. */
    eeprom_return_value = Cy_Em_EEPROM_Read(LOGICAL_EEPROM_START, eeprom_read_array,
                                          LOGICAL_EEPROM_SIZE, &Em_EEPROM_context);
    handle_error(eeprom_return_value, "Emulated EEPROM Read failed \r\n");


    /* If first byte of EEPROM is not 'P', then write the data for initializing
     * the EEPROM content.
     */
    if(ASCII_INTIAL != eeprom_read_array[0])
    {
        /* Write initial data to EEPROM. */
        eeprom_return_value = Cy_Em_EEPROM_Write(LOGICAL_EEPROM_START,
                                               eeprom_write_array,
                                               RESET_COUNT_LOCATION,
                                               &Em_EEPROM_context);
        eeprom_return_value = Cy_Em_EEPROM_Write(RESET_COUNT_LOCATION,
                                               (uint8_t[2]){0, 0},
                                               COUNT_SIZE,
                                               &Em_EEPROM_context);
        handle_error(eeprom_return_value, "Emulated EEPROM Write failed \r\n");

        return 0;
    }
    
    tampers_count = eeprom_read_array[RESET_COUNT_LOCATION] * 256 + eeprom_read_array[RESET_COUNT_LOCATION+1];
    return tampers_count;
}

uint16_t increaseTamperCount(void) {
    int count;
    /* Return status for EEPROM. */
    cy_en_em_eeprom_status_t eeprom_return_value;
    
    /* Read 15 bytes out of EEPROM memory. */
    eeprom_return_value = Cy_Em_EEPROM_Read(LOGICAL_EEPROM_START, eeprom_read_array,
                                          LOGICAL_EEPROM_SIZE, &Em_EEPROM_context);
    handle_error(eeprom_return_value, "Emulated EEPROM Read failed \r\n");


    /* If first byte of EEPROM is not 'P', then write the data for initializing
     * the EEPROM content.
     */
    if(ASCII_INTIAL != eeprom_read_array[0])
    {
        /* Write initial data to EEPROM. */
        eeprom_return_value = Cy_Em_EEPROM_Write(LOGICAL_EEPROM_START,
                                               eeprom_write_array,
                                               RESET_COUNT_LOCATION,
                                               &Em_EEPROM_context);
        eeprom_return_value = Cy_Em_EEPROM_Write(RESET_COUNT_LOCATION,
                                               (uint8_t[2]){0, 0},
                                               COUNT_SIZE,
                                               &Em_EEPROM_context);
        handle_error(eeprom_return_value, "Emulated EEPROM Write failed \r\n");

        return 0;
    }

    else
    {   
        tampers_count = eeprom_read_array[RESET_COUNT_LOCATION] * 256 + eeprom_read_array[RESET_COUNT_LOCATION+1];
        tampers_count += 1;

        uint8_t count_array[2] = {(tampers_count-tampers_count%256)/256, tampers_count%256};


        /* Only update the two count values in the EEPROM. */
        eeprom_return_value = Cy_Em_EEPROM_Write(RESET_COUNT_LOCATION,
                                               count_array,
                                               COUNT_SIZE,
                                               &Em_EEPROM_context);
        handle_error(eeprom_return_value, "Emulated EEPROM Write failed \r\n");

        return tampers_count;
    }
}

void setTamperCount(uint16_t val) {
    cy_en_em_eeprom_status_t eeprom_return_value;

    eeprom_return_value = Cy_Em_EEPROM_Write(LOGICAL_EEPROM_START,
                                             eeprom_write_array,
                                             RESET_COUNT_LOCATION,
                                             &Em_EEPROM_context);
    eeprom_return_value = Cy_Em_EEPROM_Write(RESET_COUNT_LOCATION,
                                             &val,
                                             sizeof val,
                                             &Em_EEPROM_context);

    handle_error(eeprom_return_value, "Emulated EEPROM Write failed \r\n");
}

/*******************************************************************************
* Function Name: handle_error
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
void handle_error(uint32_t status, char *message)
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
