#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#include "cyhal.h"
#include "cybsp.h"
#include "cy_pdl.h"
#include "cy_retarget_io.h"

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

#include "cybt_platform_trace.h"
#include "wiced_bt_gatt.h"
#include "wiced_bt_ble.h"
#include "wiced_bt_cfg.h"
#include "wiced_bt_stack.h"
#include "cycfg_bt_settings.h"
#include "cycfg_gap.h"
#include "cycfg_gatt_db.h"

#include "ansi.h"
#include "bt_utils.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif


char bt_enabled = 0;
char bt_connected = 0;

TaskHandle_t blinky_task_handle;
TaskHandle_t bt_task_handle;
TaskHandle_t advertise_task_handle;

cyhal_lptimer_t blinky_lptimer;
char blinky_lptimer_trignext = 1;
char blinky_lptimer_gpionext = 0;
void blinky_timer_hal_cb(void *arg, cyhal_lptimer_event_t event) {
	if (blinky_lptimer_trignext) {
		cyhal_lptimer_set_delay((cyhal_lptimer_t*)arg, 16384);

		BaseType_t real = pdFALSE;
		vTaskNotifyGiveFromISR(blinky_task_handle, &real);
		portYIELD_FROM_ISR(&real);
	} else {
		Cy_GPIO_Write(GPIO_PRT13, 7, blinky_lptimer_gpionext);
	}
}


void blinky_task(void *arg) {
	Cy_GPIO_Pin_FastInit(GPIO_PRT13, 7, CY_GPIO_DM_STRONG_IN_OFF, 1, P13_7_GPIO);

	cyhal_lptimer_init(&blinky_lptimer);
	cyhal_lptimer_register_callback(&blinky_lptimer, blinky_timer_hal_cb, &blinky_lptimer);
	cyhal_lptimer_enable_event(&blinky_lptimer, CYHAL_LPTIMER_COMPARE_MATCH, 7, 1);
	cyhal_lptimer_set_match(&blinky_lptimer, 16384);
	blinky_lptimer_gpionext = 1;
	blinky_lptimer_trignext = 0;

	for (;;) {
		ulTaskNotifyTake(pdFALSE, portMAX_DELAY);

		Cy_GPIO_Inv(GPIO_PRT13, 7);
	}
}


void userbutton_isr() {
	Cy_GPIO_ClearInterrupt(GPIO_PRT0, 4);
	NVIC_ClearPendingIRQ(ioss_interrupts_gpio_0_IRQn);

	BaseType_t higherPriorityTaskWoken = pdFALSE;
	vTaskNotifyGiveFromISR(advertise_task_handle, &higherPriorityTaskWoken);
	portYIELD_FROM_ISR(higherPriorityTaskWoken);
}


wiced_bt_gatt_status_t gatt_mgmt_cb(wiced_bt_gatt_evt_t event, wiced_bt_gatt_event_data_t *data) {
	LOG_DEBUG("%s (%d)\r\n", get_gatt_event_name(event), event);
	char tmp0 = 0;

	switch (event) {
		case GATT_CONNECTION_STATUS_EVT:
			if (data->connection_status.connected) {
				bt_connected = 1;
				LOG_INFO("Connected to bluetooth device over transport type %d (cid: %u)\n", data->connection_status.transport, data->connection_status.conn_id);
			} else {
				bt_connected = 0;
				LOG_INFO("Disconnected from bluetooth device (cid: %u); reason %s (%d)\n", data->connection_status.conn_id, get_gatt_disconn_reason_name(data->connection_status.reason), data->connection_status.reason);

				app_service_characteristic_client_char_config[0] = 0;
				xTaskNotifyGive(advertise_task_handle);
			}

			break;
		case GATT_ATTRIBUTE_REQUEST_EVT: {
			wiced_bt_gatt_attribute_request_t *req = &data->attribute_request;
			char handled = 0;
			switch (req->opcode) {
				case GATT_REQ_MTU:
					handled++;
					LOG_INFO("MTU requested\n");
					LOG_DEBUG("Their MTU: %u, our MTU: %u\n", req->data.remote_mtu, CY_BT_MTU_SIZE);
					wiced_bt_gatt_server_send_mtu_rsp(req->conn_id, req->data.remote_mtu, CY_BT_MTU_SIZE);
					break;


				case GATT_REQ_READ:
				case GATT_REQ_READ_BLOB:
					handled++;
					LOG_INFO("Read requested for handle %u", req->data.read_req.handle);
					for (int i = 0; i < app_gatt_db_ext_attr_tbl_size; i++) {
						if (app_gatt_db_ext_attr_tbl[i].handle != req->data.read_req.handle) continue;
						tmp0 = 1;

						LOG_NOFMT(" (entry %u)\n", i);

						gatt_db_lookup_table_t rsp = app_gatt_db_ext_attr_tbl[i];
						wiced_bt_gatt_server_send_read_handle_rsp(req->conn_id, req->opcode, rsp.cur_len, rsp.p_data, 0);

						break;
					}

					if (!tmp0) LOG_NOFMT("\n");
					break;

				case GATT_REQ_READ_BY_TYPE: {
					handled++;
					LOG_INFO("Read by type from %u (%04x) to %u (%04x), UUID length %d, bytes",
							req->data.read_by_type.s_handle, req->data.read_by_type.s_handle,
							req->data.read_by_type.e_handle, req->data.read_by_type.e_handle,
							req->data.read_by_type.uuid.len);
					for (int i = 0; i < 16; i++) {
						LOG_NOFMT(" %02x", req->data.read_by_type.uuid.uu.uuid128[i]);
					}
					LOG_NOFMT("\n");

					LOG_INFO("req len %d\n", req->len_requested);

					uint16_t handle = req->data.read_by_type.s_handle;
					uint8_t *rsp = pvPortMalloc(req->len_requested);
					if (!rsp) {
						LOG_ERR("Error while allocating space for response\n");
						break;
					}
					LOG_DEBUG("Response allocated with pointer %p\n", rsp);
					uint8_t pair_len = 0;
					int space = req->len_requested;
					int total_used = 0;
					while (1) {

						handle = wiced_bt_gatt_find_handle_by_type(handle, req->data.read_by_type.e_handle, &req->data.read_by_type.uuid);
						
						if (!handle) break;

						int i;
						for (i = 0; i < app_gatt_db_ext_attr_tbl_size; i++) {
							if (app_gatt_db_ext_attr_tbl[i].handle == handle) break;
						}
						gatt_db_lookup_table_t *entry = &app_gatt_db_ext_attr_tbl[i];

						int used = wiced_bt_gatt_put_read_by_type_rsp_in_stream(rsp + total_used, space, &pair_len, handle, entry->cur_len, entry->p_data);
						LOG_DEBUG("Put %d bytes into stream for handle %u: ", used, handle);
						if (used == 0) {
							LOG_NOFMT("\n");
							LOG_ERR("Response full; no bytes written\n");
							break;
						}
						for (i = total_used; i < total_used + used; i++) {
							LOG_NOFMT("%02x ", rsp[i]);
						}
						LOG_NOFMT("\n");

						LOG_DEBUG("pair_len: %d\n", pair_len);

						space -= used;
						total_used += used;

						handle++;
					}
					wiced_bt_gatt_status_t gatt_status = wiced_bt_gatt_server_send_read_by_type_rsp(req->conn_id, req->opcode, pair_len, total_used, rsp, vPortFree);
					if (gatt_status == WICED_BT_GATT_SUCCESS) {
						LOG_INFO("Sent read-by-type response successfully\n");
					} else {
						LOG_INFO("Sending read-by-type response failed: %s (%d)\n", get_gatt_status_name(gatt_status), gatt_status);
						vPortFree(rsp);
					}
				}
				break;


				case GATT_REQ_WRITE: {
					handled++;
					LOG_INFO("Received write request, len %d, handle %u, offset %u\n", req->len_requested, req->data.write_req.handle, req->data.write_req.offset);
					LOG_INFO(" --> NOTE: This application is using non-standard behaviour for write requests\n");

                    // Client sends us handle number, but we need to find the actual variable with
                    // the handle number. This loop finds and stores it `entry' and `entry_index'
					wiced_bt_gatt_write_req_t *reqobj = &req->data.write_req;
					uint16_t handle = reqobj->handle;
					int entry_index;
					gatt_db_lookup_table_t *entry;
					for (entry_index = 0; entry_index < app_gatt_db_ext_attr_tbl_size; entry_index++) {
						if (app_gatt_db_ext_attr_tbl[entry_index].handle == handle) break;
					}

					if (entry_index > app_gatt_db_ext_attr_tbl_size) {
						LOG_WARN("Requested characteristic does not exist\n");
						break;
					}

					entry = &app_gatt_db_ext_attr_tbl[entry_index];

                    // Writes to CCCD should be handled normally.
					if (handle == HDLD_SERVICE_CHARACTERISTIC_CLIENT_CHAR_CONFIG) {
						int end = reqobj->offset + reqobj->val_len - 1;
						if (reqobj->offset >= entry->max_len) {
							LOG_WARN("Requested write area out of range (offset): %d >= %d\n", reqobj->offset, entry->max_len);
							wiced_bt_gatt_server_send_error_rsp(req->conn_id, req->opcode, handle, WICED_BT_GATT_INVALID_OFFSET);
							break;
						}

						if (end >= entry->max_len) {
							LOG_WARN("Requested write area out of range (end byte): %d >= %d\n", end, entry->max_len);
							wiced_bt_gatt_server_send_error_rsp(req->conn_id, req->opcode, handle, WICED_BT_GATT_INVALID_ATTR_LEN);
							break;
						}

						LOG_DEBUG("Writing bytes to handle %u (entry %d): ", handle, entry_index);

						int si = 0;
						for (int i = reqobj->offset; i <= end; i++) {
							LOG_NOFMT("%02x ", reqobj->p_val[si]);
							entry->p_data[i] = reqobj->p_val[si++];
						}
						LOG_NOFMT("\n");

						LOG_INFO("Wrote %d bytes to handle %u (entry %d)\n", si, handle, entry_index);
					} else {
                        // Ideally you'd add a check here to ensure that the client is writing to the
                        // characteristic you want to implement custom behaviour on.
                        // Something like:
                        //
                        //      else if (handle == HDLC_SERVICE_CHARACTERISTIC_VALUE) {
                        //

						if (reqobj->val_len == 0) {
							LOG_WARN("Client sent empty operation\n");
							wiced_bt_gatt_server_send_error_rsp(req->conn_id, req->opcode, handle, WICED_BT_GATT_INVALID_ATTR_LEN);
							break;
						}

						uint8_t *raw = reqobj->p_val;
						// uint16_t len = reqobj->val_len;
						
						char op = raw[0];


						switch (op) {
							case 0: {
								LOG_INFO("Operation 0: Hello World\n");
								char msg[] = "Hello World!";
								memcpy(app_service_characteristic, msg, sizeof(msg));

                                // set remaining bytes to 0
								memset(&app_service_characteristic[sizeof msg], 0, app_service_characteristic_len - sizeof msg);
							}
							break;

							case 1: {
								LOG_INFO("Operation 1: RTC time\n");
								cy_stc_rtc_config_t timenow = {};
								Cy_RTC_GetDateAndTime(&timenow);
								struct tm m_time = {
									.tm_sec = timenow.sec,
									.tm_min = timenow.min,
									.tm_hour = timenow.hour,
									.tm_mday = timenow.date,
									.tm_mon = timenow.month - 1,
									.tm_year = timenow.year + 100,
									.tm_wday = timenow.dayOfWeek
								};
								char timebuf[32] = {};
                                // strftime returns strlen(timebuf)
								int timebuf_len = strftime(timebuf, 33, "%a %d %b %Y %T", &m_time);
								memcpy(app_service_characteristic, timebuf, 32);

                                // set remaining bytes to 0
								memset(&app_service_characteristic[timebuf_len], 0, app_service_characteristic_len - timebuf_len);
							}
							break;

							default: {
								LOG_WARN("Unknown operation\n");
								char msg[] = "Unknown operation";
								memcpy(app_service_characteristic, msg, sizeof msg);

                                // set remaining bytes to 0
								memset(&app_service_characteristic[sizeof msg], 0, app_service_characteristic_len - sizeof msg);
							}
							break;
						}
					}


                    // Tell client we successfully 'wrote' the value
					LOG_DEBUG("Sending write response\n");
					wiced_bt_gatt_status_t gatt_status = wiced_bt_gatt_server_send_write_rsp(req->conn_id, req->opcode, handle);
					if (gatt_status == WICED_BT_GATT_SUCCESS) {
						LOG_INFO("Sent write response\n");
					} else {
						LOG_ERR("Sending write response failed with %s\n", get_gatt_status_name(gatt_status));
						return gatt_status;
					}

                    // Send client notifications if the value was updated
					if (handle == HDLC_SERVICE_CHARACTERISTIC_VALUE) {
						switch (app_service_characteristic_client_char_config[0] & 3) {
							case 0:
								LOG_WARN("Client has notifications and indications turned off\n");
								break;

							case 3:
							case 1:
								LOG_DEBUG("Sending client notification for updated field\n");
								gatt_status = wiced_bt_gatt_server_send_notification(req->conn_id, handle, entry->max_len, entry->p_data, 0);
								if (gatt_status == WICED_BT_GATT_SUCCESS) {
									LOG_INFO("Sent notification\n");
								} else {
									LOG_ERR("Sending notification failed with %s\n", get_gatt_status_name(gatt_status));
									return gatt_status;
								}
								break;

							case 2:
								LOG_DEBUG("Sending client indication for updated field\n");
								gatt_status = wiced_bt_gatt_server_send_notification(req->conn_id, handle, entry->max_len, entry->p_data, 0);
								if (gatt_status == WICED_BT_GATT_SUCCESS) {
									LOG_INFO("Sent indication\n");
								} else {
									LOG_ERR("Sending indication failed with %s\n", get_gatt_status_name(gatt_status));
									return gatt_status;
								}
								break;
						}
					}
				}
				break;

				default:
					wiced_bt_gatt_server_send_error_rsp(req->conn_id, req->opcode, 0, WICED_BT_GATT_NOT_IMPLEMENTED);
			}

			if (!handled) {
				LOG_WARN("Opcode %s (%d) unhandled\n", get_gatt_opcode_name(req->opcode), req->opcode);
			}
		}
		break;

		case GATT_GET_RESPONSE_BUFFER_EVT:
			LOG_DEBUG("Application requested buffer of %u bytes\n", data->buffer_request.len_requested);
			data->buffer_request.buffer.p_app_ctxt = vPortFree;
			data->buffer_request.buffer.p_app_rsp_buffer = pvPortMalloc(data->buffer_request.len_requested);
			break;

		case GATT_APP_BUFFER_TRANSMITTED_EVT: {
			LOG_DEBUG("App data %p, context %p\n", data->buffer_xmitted.p_app_data, data->buffer_xmitted.p_app_ctxt);

			if (!data->buffer_xmitted.p_app_ctxt) break;

			LOG_DEBUG("Calling context\n");

			((void(*)(uint8_t*))data->buffer_xmitted.p_app_ctxt)(data->buffer_xmitted.p_app_data);
		} 
		break;

		case GATT_OPERATION_CPLT_EVT:
			LOG_DEBUG("GATT operation %s complete with status %s\n", get_gatt_optype_name(data->operation_complete.op), get_gatt_status_name(data->operation_complete.conn_id));
			break;

		case GATT_DISCOVERY_RESULT_EVT:
			LOG_DEBUG("Discovery result type: %s (%d)\n", get_gatt_discovery_type_name(data->discovery_result.discovery_type), data->discovery_result.discovery_type);
			break;

		case GATT_DISCOVERY_CPLT_EVT:
			LOG_DEBUG("Discovery for %s complete with %s (%d)\n", get_gatt_discovery_type_name(data->discovery_complete.discovery_type), get_gatt_status_name(data->discovery_complete.status), data->discovery_complete.status);
			break;

		case GATT_CONGESTION_EVT:
			LOG_WARN("Congestion event yes/no: %d\n", data->congestion.congested);
			break;

	}

	return WICED_BT_GATT_SUCCESS;
}

wiced_result_t bt_mgmt_cb(wiced_bt_management_evt_t event, wiced_bt_management_evt_data_t *event_data) {
	switch (event) {
		case BTM_ENABLED_EVT: {
				bt_enabled = 1;
				LOG_DEBUG("%s (%d)\r\n" , get_btm_event_name(event), event);

				LOG_INFO("Bluetooth enabled\r\n");


				wiced_bt_gatt_status_t gatt_status = wiced_bt_gatt_db_init(gatt_database, gatt_database_len, 0);
				if (gatt_status != WICED_BT_GATT_SUCCESS) {
					LOG_ERR("GATT DB initialisation failed: %s (%d)\r\n", get_gatt_status_name(gatt_status), gatt_status);
				} else {
					LOG_INFO("GATT DB initialisation success\r\n");
				}

				gatt_status = wiced_bt_gatt_register(gatt_mgmt_cb);
				if (gatt_status != WICED_BT_GATT_SUCCESS) {
					LOG_ERR("GATT DB callback registration failed: %s (%d)\r\n", get_gatt_status_name(gatt_status), gatt_status);
				} else {
					LOG_INFO("GATT DB callback registration success\r\n");
				}

				wiced_bt_set_pairable_mode(0, 0);

				wiced_result_t wiced_result = wiced_bt_ble_set_raw_advertisement_data(3, cy_bt_adv_packet_data);
				if (wiced_result != WICED_SUCCESS) {
					LOG_ERR("Setting data for advertisement error: %d\n", wiced_result);
				} else {
					LOG_INFO("Advertisement data set\n");
				}

				xTaskNotifyGive(advertise_task_handle);
				
			}
			break;

		case BTM_BLE_ADVERT_STATE_CHANGED_EVT:
			LOG_INFO("Advertisement state changed to %s\n", get_ble_advert_mode_name(event_data->ble_advert_state_changed));

			if (event_data->ble_advert_state_changed == BTM_BLE_ADVERT_OFF) {
				blinky_lptimer_gpionext = !bt_connected;
				blinky_lptimer_trignext = 0;
			} else {
				blinky_lptimer_gpionext = 0;
				blinky_lptimer_trignext = 1;
				cyhal_lptimer_set_delay(&blinky_lptimer, 16384);
			}

			break;

		default:
			LOG_WARN("Unhandled event %s (%d)\r\n", get_btm_event_name(event), event);
	}

	return WICED_BT_SUCCESS;
}

void bt_task(void *arg) {
	LOG_CLEARFMT();
	cybt_platform_config_init(&cybsp_bt_platform_cfg);
	wiced_result_t status = wiced_bt_stack_init(bt_mgmt_cb, &wiced_bt_cfg_settings);
	if (status != WICED_SUCCESS) {
		LOG_ERR("Bluetooth stack initialisation failed with %s (%d)\n", get_wiced_result_name(status), status);
	} else {
		LOG_INFO("Bluetooth stack initialisation successful\n");
	}


	for (;;) {
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
	}
}

void advertise_task(void *arg) {
	wiced_result_t wiced_result;
	for (;;) {
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		if (!bt_enabled) {
			LOG_WARN("Advertisement task called before bluetooth initialisation\n");
		}

		wiced_bt_ble_advert_mode_t current_mode = wiced_bt_ble_get_current_advert_mode();
		if (current_mode == BTM_BLE_ADVERT_OFF) {
			if (!bt_connected) {
				wiced_result = wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH, BLE_ADDR_PUBLIC, cy_bt_device_address);
				if (wiced_result != WICED_SUCCESS) {
					LOG_ERR("Advertisement start error: %s (%d)\n", get_wiced_result_name(wiced_result), wiced_result);
				} else {
					LOG_INFO("Advertisement started\r\n");
				}
			} else {
				LOG_WARN("Advertisement task called when device was already connected; not starting advertisements\n");
			}
		} else {
			wiced_result = wiced_bt_start_advertisements(BTM_BLE_ADVERT_OFF, BLE_ADDR_PUBLIC, cy_bt_device_address);
			if (wiced_result != WICED_SUCCESS) {
				LOG_ERR("Advertisement stop error: %s (%d)\n", get_wiced_result_name(wiced_result), wiced_result);
			} else {
				LOG_INFO("Advertisement stopped\r\n");
				blinky_lptimer_gpionext = 1;
				blinky_lptimer_trignext = 0;
			}
		}
	}
}

int main() {
	cybsp_init();
	__enable_irq();

    Cy_SysClk_PllDisable(1);
    Cy_SysClk_FllDisable();

    SystemCoreClockUpdate();
    Cy_SysLib_SetWaitStates(1, 8000000);

    Cy_SysPm_BuckEnable(CY_SYSPM_BUCK_OUT1_VOLTAGE_ULP);

	cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, 115200);

	LOG_NOFMT("\r\n");

	if (pdPASS == xTaskCreate(blinky_task, "blinky task", configMINIMAL_STACK_SIZE * 8, 0, configMAX_PRIORITIES - 2, &blinky_task_handle)) {
		LOG_DEBUG("Created blinky task\r\n");
	}

	if (pdPASS == xTaskCreate(bt_task, "bt task", configMINIMAL_STACK_SIZE * 8, 0, configMAX_PRIORITIES - 3, &bt_task_handle)) {
		LOG_DEBUG("Created bluetooth task\r\n");
	}

	if (pdPASS == xTaskCreate(advertise_task, "bt task", configMINIMAL_STACK_SIZE * 8, 0, configMAX_PRIORITIES - 3, &advertise_task_handle)) {
		LOG_DEBUG("Created advertisement task\r\n");
	}

	Cy_GPIO_Pin_FastInit(GPIO_PRT0, 4, CY_GPIO_DM_PULLUP, 1, P0_4_GPIO);
	Cy_GPIO_SetInterruptEdge(GPIO_PRT0, 4, CY_GPIO_INTR_FALLING);
	Cy_GPIO_SetInterruptMask(GPIO_PRT0, 4, 1);
	
	cy_stc_sysint_t ub_intr_cfg = {
		.intrSrc = ioss_interrupts_gpio_0_IRQn,
		.intrPriority = 7
	};

	Cy_SysInt_Init(&ub_intr_cfg, userbutton_isr);
	NVIC_EnableIRQ(ub_intr_cfg.intrSrc);

	Cy_RTC_SyncFromRtc();

	vTaskStartScheduler();
}
