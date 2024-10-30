#ifndef BT_UTILS_H
#define BT_UTILS_H

#include "wiced_bt_stack.h"
#include "wiced_bt_gatt.h"

const char *get_btm_event_name(wiced_bt_management_evt_t event);
const char *get_gatt_event_name(wiced_bt_gatt_evt_t event);
const char *get_ble_advert_mode_name(wiced_bt_ble_advert_mode_t mode);
const char *get_gatt_discovery_type_name(wiced_bt_gatt_discovery_type_t type);
const char *get_gatt_status_name(wiced_bt_gatt_status_t status);
const char *get_gatt_disconn_reason_name(wiced_bt_gatt_disconn_reason_t reason);
const char *get_wiced_result_name(wiced_result_t result);
const char *get_gatt_opcode_name(wiced_bt_gatt_opcode_t opcode);
const char *get_gatt_optype_name(wiced_bt_gatt_optype_t optype);

void print_bd_address(const char * msg, wiced_bt_device_address_t bdaddr);
void print_local_bd_address(void);

#endif
