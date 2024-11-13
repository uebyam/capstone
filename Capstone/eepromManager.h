#ifndef EEPROM_H
#define EEPROM_H

#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "cy_em_eeprom.h"

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
#define MAX_TIMESTAMP_COUNT (40)

#define TAMPER_TYPE_LOCATION (TIMESTAMP_LOCATION + TIMESTAMP_SIZE * MAX_TIMESTAMP_COUNT)
#define TAMPER_TYPE_SIZE (1u)

/* Logical Size of Emulated EEPROM in bytes. */
#define LOGICAL_EEPROM_SIZE     (TAMPER_TYPE_LOCATION + MAX_TIMESTAMP_COUNT * TAMPER_TYPE_SIZE)
#define LOGICAL_EEPROM_START    (0u)

/* EEPROM Configuration details. All the sizes mentioned are in bytes.
 * For details on how to configure these values refer to cy_em_eeprom.h. The
 * library documentation is provided in Em EEPROM API Reference Manual. The user
 * access it from ModusToolbox IDE Quick Panel > Documentation> 
 * Cypress Em_EEPROM middleware API reference manual
 */
#define EEPROM_SIZE             (LOGICAL_EEPROM_SIZE)
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


enum {
	EEPROM_TAMPER_TYPE_UNKNOWN = 0,
    EEPROM_TAMPER_TYPE_BATTERY_LIFT = 1,
    EEPROM_TAMPER_TYPE_UART_DISCONNECT = 2,
    EEPROM_TAMPER_TYPE_BATTERY_VOLTAGE = 3,
};
typedef uint8_t eeprom_tamper_type_t;


/*******************************************************************************
 * Function Prototypes
 ******************************************************************************/
void initEEPROM(void);
void setTamperCount(uint8_t val);
uint8_t getTamperCount(void);
uint8_t increaseTamperCount(eeprom_tamper_type_t type);
void getTimestamps(int *timestamps, uint8_t *tamper_types, size_t offset, size_t count);
void updateTamperTimestamps(time_t offset);

/*******************************************************************************
 * External Global Variables
 ******************************************************************************/
extern cy_stc_eeprom_config_t Em_EEPROM_config;
extern cy_stc_eeprom_context_t Em_EEPROM_context;
extern const uint8_t eeprom_storage[];

#endif /* EEPROM_H */
