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
void initEEPROM(void);
void setTamperCount(uint16_t val);
uint16_t getTamperCount(void);
uint16_t increaseTamperCount(void);
void getTimestamps(int *timestamps);

/*******************************************************************************
 * External Global Variables
 ******************************************************************************/
extern cy_stc_eeprom_config_t Em_EEPROM_config;
extern cy_stc_eeprom_context_t Em_EEPROM_context;
extern const uint8_t eeprom_storage[];

#endif /* EEPROM_H */
