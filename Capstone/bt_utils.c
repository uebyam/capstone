#include "bt_utils.h"
#include "ansi.h"

#include <stdio.h>

#define CASE_RETURN_STR(const)          case const: return #const;
const char *get_btm_event_name(wiced_bt_management_evt_t event)
{

    switch ( (int)event )
    {

    CASE_RETURN_STR(BTM_ENABLED_EVT)
    CASE_RETURN_STR(BTM_DISABLED_EVT)
    CASE_RETURN_STR(BTM_POWER_MANAGEMENT_STATUS_EVT)
    CASE_RETURN_STR(BTM_PIN_REQUEST_EVT)
    CASE_RETURN_STR(BTM_USER_CONFIRMATION_REQUEST_EVT)
    CASE_RETURN_STR(BTM_PASSKEY_NOTIFICATION_EVT)
    CASE_RETURN_STR(BTM_PASSKEY_REQUEST_EVT)
    CASE_RETURN_STR(BTM_KEYPRESS_NOTIFICATION_EVT)
    CASE_RETURN_STR(BTM_PAIRING_IO_CAPABILITIES_BR_EDR_REQUEST_EVT)
    CASE_RETURN_STR(BTM_PAIRING_IO_CAPABILITIES_BR_EDR_RESPONSE_EVT)
    CASE_RETURN_STR(BTM_PAIRING_IO_CAPABILITIES_BLE_REQUEST_EVT)
    CASE_RETURN_STR(BTM_PAIRING_COMPLETE_EVT)
    CASE_RETURN_STR(BTM_ENCRYPTION_STATUS_EVT)
    CASE_RETURN_STR(BTM_SECURITY_REQUEST_EVT)
    CASE_RETURN_STR(BTM_SECURITY_FAILED_EVT)
    CASE_RETURN_STR(BTM_SECURITY_ABORTED_EVT)
    CASE_RETURN_STR(BTM_READ_LOCAL_OOB_DATA_COMPLETE_EVT)
    CASE_RETURN_STR(BTM_REMOTE_OOB_DATA_REQUEST_EVT)
    CASE_RETURN_STR(BTM_PAIRED_DEVICE_LINK_KEYS_UPDATE_EVT)
    CASE_RETURN_STR(BTM_PAIRED_DEVICE_LINK_KEYS_REQUEST_EVT)
    CASE_RETURN_STR(BTM_LOCAL_IDENTITY_KEYS_UPDATE_EVT)
    CASE_RETURN_STR(BTM_LOCAL_IDENTITY_KEYS_REQUEST_EVT)
    CASE_RETURN_STR(BTM_BLE_SCAN_STATE_CHANGED_EVT)
    CASE_RETURN_STR(BTM_BLE_ADVERT_STATE_CHANGED_EVT)
    CASE_RETURN_STR(BTM_SMP_REMOTE_OOB_DATA_REQUEST_EVT)
    CASE_RETURN_STR(BTM_SMP_SC_REMOTE_OOB_DATA_REQUEST_EVT)
    CASE_RETURN_STR(BTM_SMP_SC_LOCAL_OOB_DATA_NOTIFICATION_EVT)
    CASE_RETURN_STR(BTM_SCO_CONNECTED_EVT)
    CASE_RETURN_STR(BTM_SCO_DISCONNECTED_EVT)
    CASE_RETURN_STR(BTM_SCO_CONNECTION_REQUEST_EVT)
    CASE_RETURN_STR(BTM_SCO_CONNECTION_CHANGE_EVT)
    CASE_RETURN_STR(BTM_BLE_CONNECTION_PARAM_UPDATE)
    CASE_RETURN_STR(BTM_BLE_DATA_LENGTH_UPDATE_EVENT)
#ifdef CYW20819A1
    CASE_RETURN_STR(BTM_BLE_PHY_UPDATE_EVT)
#endif
    }

    return "UNKNOWN_EVENT";
}

const char *get_gatt_event_name(wiced_bt_gatt_evt_t event) {
	switch (event) {
		CASE_RETURN_STR(GATT_CONNECTION_STATUS_EVT)
		CASE_RETURN_STR(GATT_OPERATION_CPLT_EVT)
		CASE_RETURN_STR(GATT_DISCOVERY_RESULT_EVT)
		CASE_RETURN_STR(GATT_DISCOVERY_CPLT_EVT)
		CASE_RETURN_STR(GATT_ATTRIBUTE_REQUEST_EVT)
		CASE_RETURN_STR(GATT_CONGESTION_EVT)
		CASE_RETURN_STR(GATT_GET_RESPONSE_BUFFER_EVT)
		CASE_RETURN_STR(GATT_APP_BUFFER_TRANSMITTED_EVT)
	}

	return "UNKNOWN_EVENT";
}

const char *get_ble_advert_mode_name(wiced_bt_ble_advert_mode_t mode) {
	switch (mode) {
		CASE_RETURN_STR(BTM_BLE_ADVERT_OFF)
		CASE_RETURN_STR(BTM_BLE_ADVERT_DIRECTED_HIGH)
		CASE_RETURN_STR(BTM_BLE_ADVERT_DIRECTED_LOW)
		CASE_RETURN_STR(BTM_BLE_ADVERT_UNDIRECTED_HIGH)
		CASE_RETURN_STR(BTM_BLE_ADVERT_UNDIRECTED_LOW)
		CASE_RETURN_STR(BTM_BLE_ADVERT_NONCONN_HIGH)
		CASE_RETURN_STR(BTM_BLE_ADVERT_NONCONN_LOW)
		CASE_RETURN_STR(BTM_BLE_ADVERT_DISCOVERABLE_HIGH)
		CASE_RETURN_STR(BTM_BLE_ADVERT_DISCOVERABLE_LOW)
	}

	return "UNKNOWN MODE";
}

const char *get_gatt_discovery_type_name(wiced_bt_gatt_discovery_type_t type) {
	switch (type) {
		CASE_RETURN_STR(GATT_DISCOVER_SERVICES_ALL)
		CASE_RETURN_STR(GATT_DISCOVER_SERVICES_BY_UUID)
		CASE_RETURN_STR(GATT_DISCOVER_INCLUDED_SERVICES)
		CASE_RETURN_STR(GATT_DISCOVER_CHARACTERISTICS)
		CASE_RETURN_STR(GATT_DISCOVER_CHARACTERISTIC_DESCRIPTORS)
		CASE_RETURN_STR(GATT_DISCOVER_MAX)
	}
	return "UNKNOWN TYPE";
}

const char *get_gatt_status_name(wiced_bt_gatt_status_t status) {
	switch (status) {
		case WICED_BT_GATT_SUCCESS: return  "WICED_BT_GATT_SUCCESS | WICED_BT_ENCRYPTED_MITM";
		CASE_RETURN_STR(WICED_BT_GATT_INVALID_HANDLE)
		CASE_RETURN_STR(WICED_BT_GATT_READ_NOT_PERMIT)
		CASE_RETURN_STR(WICED_BT_GATT_WRITE_NOT_PERMIT)
		CASE_RETURN_STR(WICED_BT_GATT_INVALID_PDU)
		CASE_RETURN_STR(WICED_BT_GATT_INSUF_AUTHENTICATION)
		CASE_RETURN_STR(WICED_BT_GATT_REQ_NOT_SUPPORTED)
		CASE_RETURN_STR(WICED_BT_GATT_INVALID_OFFSET)
		CASE_RETURN_STR(WICED_BT_GATT_INSUF_AUTHORIZATION)
		CASE_RETURN_STR(WICED_BT_GATT_PREPARE_Q_FULL)
		CASE_RETURN_STR(WICED_BT_GATT_ATTRIBUTE_NOT_FOUND)
		CASE_RETURN_STR(WICED_BT_GATT_NOT_LONG)
		CASE_RETURN_STR(WICED_BT_GATT_INSUF_KEY_SIZE)
		CASE_RETURN_STR(WICED_BT_GATT_INVALID_ATTR_LEN)
		CASE_RETURN_STR(WICED_BT_GATT_ERR_UNLIKELY)
		CASE_RETURN_STR(WICED_BT_GATT_INSUF_ENCRYPTION)
		CASE_RETURN_STR(WICED_BT_GATT_UNSUPPORT_GRP_TYPE)
		CASE_RETURN_STR(WICED_BT_GATT_INSUF_RESOURCE)
		CASE_RETURN_STR(WICED_BT_GATT_DATABASE_OUT_OF_SYNC)
		CASE_RETURN_STR(WICED_BT_GATT_VALUE_NOT_ALLOWED)
		CASE_RETURN_STR(WICED_BT_GATT_WRITE_REQ_REJECTED)
		CASE_RETURN_STR(WICED_BT_GATT_CCCD_IMPROPER_CONFIGURED)
		CASE_RETURN_STR(WICED_BT_GATT_BUSY)
		CASE_RETURN_STR(WICED_BT_GATT_OUT_OF_RANGE)
		CASE_RETURN_STR(WICED_BT_GATT_ILLEGAL_PARAMETER)
		CASE_RETURN_STR(WICED_BT_GATT_NO_RESOURCES)
		CASE_RETURN_STR(WICED_BT_GATT_INTERNAL_ERROR)
		CASE_RETURN_STR(WICED_BT_GATT_WRONG_STATE)
		CASE_RETURN_STR(WICED_BT_GATT_DB_FULL)
		CASE_RETURN_STR(WICED_BT_GATT_UNUSED1)
		CASE_RETURN_STR(WICED_BT_GATT_ERROR)
		CASE_RETURN_STR(WICED_BT_GATT_CMD_STARTED)
		CASE_RETURN_STR(WICED_BT_GATT_PENDING)
		CASE_RETURN_STR(WICED_BT_GATT_AUTH_FAIL)
		CASE_RETURN_STR(WICED_BT_GATT_MORE)
		CASE_RETURN_STR(WICED_BT_GATT_INVALID_CFG)
		CASE_RETURN_STR(WICED_BT_GATT_SERVICE_STARTED)
		CASE_RETURN_STR(WICED_BT_GATT_ENCRYPTED_NO_MITM)
		CASE_RETURN_STR(WICED_BT_GATT_NOT_ENCRYPTED)
		CASE_RETURN_STR(WICED_BT_GATT_CONGESTED)
		CASE_RETURN_STR(WICED_BT_GATT_NOT_ALLOWED)
		CASE_RETURN_STR(WICED_BT_GATT_HANDLED)
		CASE_RETURN_STR(WICED_BT_GATT_NO_PENDING_OPERATION)
		CASE_RETURN_STR(WICED_BT_GATT_INDICATION_RESPONSE_PENDING)
		CASE_RETURN_STR(WICED_BT_GATT_UNUSED2)
		CASE_RETURN_STR(WICED_BT_GATT_CCC_CFG_ERR)
		CASE_RETURN_STR(WICED_BT_GATT_PRC_IN_PROGRESS)
	}

	return "UNKNOWN STATUS";
}

const char *get_gatt_disconn_reason_name(wiced_bt_gatt_disconn_reason_t reason) {
	switch (reason) {
		CASE_RETURN_STR(GATT_CONN_UNKNOWN)
		CASE_RETURN_STR(GATT_CONN_L2C_FAILURE)
		CASE_RETURN_STR(GATT_CONN_TIMEOUT)
		CASE_RETURN_STR(GATT_CONN_TERMINATE_PEER_USER)
		CASE_RETURN_STR(GATT_CONN_TERMINATE_LOCAL_HOST)
		CASE_RETURN_STR(GATT_CONN_FAIL_ESTABLISH)
		CASE_RETURN_STR(GATT_CONN_LMP_TIMEOUT)
		CASE_RETURN_STR(GATT_CONN_CANCEL)
	}

	return "UNKNOWN REASON";
}

const char *get_wiced_result_name(wiced_result_t result) {
	switch (result) {
		CASE_RETURN_STR(WICED_SUCCESS)
		CASE_RETURN_STR(WICED_DELETED)
		CASE_RETURN_STR(WICED_POOL_ERROR)
		CASE_RETURN_STR(WICED_PTR_ERROR)
		CASE_RETURN_STR(WICED_WAIT_ERROR)
		CASE_RETURN_STR(WICED_SIZE_ERROR)
		CASE_RETURN_STR(WICED_GROUP_ERROR)
		CASE_RETURN_STR(WICED_NO_EVENTS)
		CASE_RETURN_STR(WICED_OPTION_ERROR)
		CASE_RETURN_STR(WICED_QUEUE_ERROR)
		CASE_RETURN_STR(WICED_QUEUE_EMPTY)
		CASE_RETURN_STR(WICED_QUEUE_FULL)
		CASE_RETURN_STR(WICED_SEMAPHORE_ERROR)
		CASE_RETURN_STR(WICED_NO_INSTANCE)
		CASE_RETURN_STR(WICED_THREAD_ERROR)
		CASE_RETURN_STR(WICED_PRIORITY_ERROR)
		case WICED_START_ERROR: return "WICED_START_ERROR | WICED_NO_MEMORY";
		CASE_RETURN_STR(WICED_DELETE_ERROR)
		CASE_RETURN_STR(WICED_RESUME_ERROR)
		CASE_RETURN_STR(WICED_CALLER_ERROR)
		CASE_RETURN_STR(WICED_SUSPEND_ERROR)
		CASE_RETURN_STR(WICED_TIMER_ERROR)
		CASE_RETURN_STR(WICED_TICK_ERROR)
		CASE_RETURN_STR(WICED_ACTIVATE_ERROR)
		CASE_RETURN_STR(WICED_THRESH_ERROR)
		CASE_RETURN_STR(WICED_SUSPEND_LIFTED)
		CASE_RETURN_STR(WICED_WAIT_ABORTED)
		CASE_RETURN_STR(WICED_WAIT_ABORT_ERROR)
		CASE_RETURN_STR(WICED_MUTEX_ERROR)
		CASE_RETURN_STR(WICED_NOT_AVAILABLE)
		CASE_RETURN_STR(WICED_NOT_OWNED)
		CASE_RETURN_STR(WICED_INHERIT_ERROR)
		CASE_RETURN_STR(WICED_NOT_DONE)
		CASE_RETURN_STR(WICED_CEILING_EXCEEDED)
		CASE_RETURN_STR(WICED_INVALID_CEILING)
		CASE_RETURN_STR(WICED_STA_JOIN_FAILED)
		CASE_RETURN_STR(WICED_SLEEP_ERROR)
		CASE_RETURN_STR(WICED_PENDING)
		CASE_RETURN_STR(WICED_TIMEOUT)
		CASE_RETURN_STR(WICED_PARTIAL_RESULTS)
		CASE_RETURN_STR(WICED_ERROR)
		CASE_RETURN_STR(WICED_BADARG)
		CASE_RETURN_STR(WICED_BADOPTION)
		CASE_RETURN_STR(WICED_UNSUPPORTED)
		CASE_RETURN_STR(WICED_OUT_OF_HEAP_SPACE)
		CASE_RETURN_STR(WICED_NOTUP)
		CASE_RETURN_STR(WICED_UNFINISHED)
		CASE_RETURN_STR(WICED_CONNECTION_LOST)
		CASE_RETURN_STR(WICED_NOT_FOUND)
		CASE_RETURN_STR(WICED_PACKET_BUFFER_CORRUPT)
		CASE_RETURN_STR(WICED_ROUTING_ERROR)
		CASE_RETURN_STR(WICED_BADVALUE)
		CASE_RETURN_STR(WICED_WOULD_BLOCK)
		CASE_RETURN_STR(WICED_ABORTED)
		CASE_RETURN_STR(WICED_CONNECTION_RESET)
		CASE_RETURN_STR(WICED_CONNECTION_CLOSED)
		CASE_RETURN_STR(WICED_NOT_CONNECTED)
		CASE_RETURN_STR(WICED_ADDRESS_IN_USE)
		CASE_RETURN_STR(WICED_NETWORK_INTERFACE_ERROR)
		CASE_RETURN_STR(WICED_ALREADY_CONNECTED)
		CASE_RETURN_STR(WICED_INVALID_INTERFACE)
		CASE_RETURN_STR(WICED_SOCKET_CREATE_FAIL)
		CASE_RETURN_STR(WICED_INVALID_SOCKET)
		CASE_RETURN_STR(WICED_CORRUPT_PACKET_BUFFER)
		CASE_RETURN_STR(WICED_UNKNOWN_NETWORK_STACK_ERROR)
		CASE_RETURN_STR(WICED_NO_STORED_AP_IN_DCT)
		CASE_RETURN_STR(WICED_ALREADY_INITIALIZED)
		CASE_RETURN_STR(WICED_FEATURE_NOT_ENABLED)
		default:
			return "UNKNOWN RESULT";
	}
}


const char *get_gatt_opcode_name(wiced_bt_gatt_opcode_t opcode) {
	switch (opcode) {
		CASE_RETURN_STR(GATT_RSP_ERROR)
		CASE_RETURN_STR(GATT_REQ_MTU)
		CASE_RETURN_STR(GATT_RSP_MTU)
		CASE_RETURN_STR(GATT_REQ_FIND_INFO)
		CASE_RETURN_STR(GATT_RSP_FIND_INFO)
		CASE_RETURN_STR(GATT_REQ_FIND_TYPE_VALUE)
		CASE_RETURN_STR(GATT_RSP_FIND_TYPE_VALUE)
		CASE_RETURN_STR(GATT_REQ_READ_BY_TYPE)
		CASE_RETURN_STR(GATT_RSP_READ_BY_TYPE)
		CASE_RETURN_STR(GATT_REQ_READ)
		CASE_RETURN_STR(GATT_RSP_READ)
		CASE_RETURN_STR(GATT_REQ_READ_BLOB)
		CASE_RETURN_STR(GATT_RSP_READ_BLOB)
		CASE_RETURN_STR(GATT_REQ_READ_MULTI)
		CASE_RETURN_STR(GATT_RSP_READ_MULTI)
		CASE_RETURN_STR(GATT_REQ_READ_BY_GRP_TYPE)
		CASE_RETURN_STR(GATT_RSP_READ_BY_GRP_TYPE)
		CASE_RETURN_STR(GATT_REQ_WRITE)
		CASE_RETURN_STR(GATT_RSP_WRITE)
		CASE_RETURN_STR(GATT_REQ_PREPARE_WRITE)
		CASE_RETURN_STR(GATT_RSP_PREPARE_WRITE)
		CASE_RETURN_STR(GATT_REQ_EXECUTE_WRITE)
		CASE_RETURN_STR(GATT_RSP_EXECUTE_WRITE)
		CASE_RETURN_STR(GATT_HANDLE_VALUE_NOTIF)
		CASE_RETURN_STR(GATT_HANDLE_VALUE_IND)
		CASE_RETURN_STR(GATT_HANDLE_VALUE_CONF)
		CASE_RETURN_STR(GATT_REQ_READ_MULTI_VAR_LENGTH)
		CASE_RETURN_STR(GATT_RSP_READ_MULTI_VAR_LENGTH)
		CASE_RETURN_STR(GATT_HANDLE_VALUE_MULTI_NOTIF)
		CASE_RETURN_STR(GATT_CMD_WRITE)
		CASE_RETURN_STR(GATT_CMD_SIGNED_WRITE)
	}
	return "UNKNOWN OPCODE";
}


const char *get_gatt_optype_name(wiced_bt_gatt_optype_t optype) {
	switch (optype) {
		CASE_RETURN_STR(GATTC_OPTYPE_NONE)
		CASE_RETURN_STR(GATTC_OPTYPE_DISCOVERY)
		CASE_RETURN_STR(GATTC_OPTYPE_READ_HANDLE)
		CASE_RETURN_STR(GATTC_OPTYPE_READ_BY_TYPE)
		CASE_RETURN_STR(GATTC_OPTYPE_READ_MULTIPLE)
		CASE_RETURN_STR(GATTC_OPTYPE_WRITE_WITH_RSP)
		CASE_RETURN_STR(GATTC_OPTYPE_WRITE_NO_RSP)
		CASE_RETURN_STR(GATTC_OPTYPE_PREPARE_WRITE)
		CASE_RETURN_STR(GATTC_OPTYPE_EXECUTE_WRITE)
		CASE_RETURN_STR(GATTC_OPTYPE_CONFIG_MTU)
		CASE_RETURN_STR(GATTC_OPTYPE_NOTIFICATION)
		CASE_RETURN_STR(GATTC_OPTYPE_INDICATION)
	}
	return "UNKNOWN OPTYPE";
}

/*
* Function Name: print_bd_address()
*
* @brief This utility function prints the address of the Bluetooth device
*
* @param wiced_bt_device_address_t  Bluetooth address
*
* @return void
*
*/
void print_bd_address(char * msg, wiced_bt_device_address_t bdaddr)
{
    LOG_INFO("%s %02X:%02X:%02X:%02X:%02X:%02X\n",msg? msg:"", bdaddr[0],
                                              bdaddr[1],
                                              bdaddr[2],
                                              bdaddr[3],
                                              bdaddr[4],
                                              bdaddr[5]);
}

/*
* Function Name: print_local_bd_address()
*
* @brief This utility function prints the address of the Bluetooth device
*
* @param wiced_bt_device_address_t  Bluetooth address
*
* @return void
*
*/
void print_local_bd_address(void)
{
    wiced_bt_device_address_t local_device_bd_addr = { 0 };

    wiced_bt_dev_read_local_addr(local_device_bd_addr);

    print_bd_address("My Bluetooth Device Address: ", local_device_bd_addr);
}
