#ifndef EEPROM_H
#define EEPROM_H

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
#define COUNT_SIZE              (2u)

/* ASCII Values */
#define ASCII_NINE              (0x39)
#define ASCII_ZERO              (0x30)
#define ASCII_INTIAL            (0x54)

/* EEPROM Configuration details */
#define EEPROM_SIZE             (256u)
#define BLOCKING_WRITE          (1u)
#define REDUNDANT_COPY          (1u)
#define WEAR_LEVELLING_FACTOR   (2u)
#define SIMPLE_MODE             (0u)

/* Flash region selection */
#define USER_FLASH              (0u)
#define EMULATED_EEPROM_FLASH   (1u)

#if CY_EM_EEPROM_SIZE
#define FLASH_REGION_TO_USE     EMULATED_EEPROM_FLASH
#else
#define FLASH_REGION_TO_USE     USER_FLASH
#endif

#define GPIO_LOW                (0u)

#if (defined(CY_DEVICE_SECURE) && (USER_FLASH == FLASH_REGION_TO_USE))
#define APP_DEFINED_EM_EEPROM_LOCATION_IN_FLASH  (0x10021000)
#endif

/*******************************************************************************
 * Function Prototypes
 ******************************************************************************/
void initEEPROM(void);
void setTamperCount(uint16_t val);
uint16_t getTamperCount(void);
uint16_t increaseTamperCount(void);

/*******************************************************************************
 * External Global Variables
 ******************************************************************************/
extern cy_stc_eeprom_config_t Em_EEPROM_config;
extern cy_stc_eeprom_context_t Em_EEPROM_context;
extern const uint8_t eeprom_storage[];
extern uint8_t eeprom_read_array[LOGICAL_EEPROM_SIZE];
extern uint8_t eeprom_write_array[RESET_COUNT_LOCATION];
extern uint16_t tampers_count;

#endif /* EEPROM_H */
