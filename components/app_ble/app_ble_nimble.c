#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"

#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "app_ble.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#ifndef LOG_TAG
#define LOG_TAG "ABLE"
#endif

#define BATTERY_SVC_UUID        0x180F
#define BATTERY_LEVEL_ATTR_UUID 0x2A19

#define APP_SVC_UUID                                                                               \
	0xFB, 0x8D, 0x2C, 0xC1, 0xE0, 0xB3, 0x4D, 0xD4, 0x9E, 0x91, 0x9A, 0xC2, 0xF0, 0xB8, 0x5A,  \
		0x0C
#define APP_ATTR_READ_UUID                                                                         \
	0xFB, 0x8D, 0x2C, 0xC1, 0xE0, 0xB3, 0x4D, 0xD4, 0x9E, 0x91, 0x9B, 0xC2, 0xF0, 0xB8, 0x5A,  \
		0x0C
#define APP_ATTR_WRITE_UUID                                                                        \
	0xFB, 0x8D, 0x2C, 0xC1, 0xE0, 0xB3, 0x4D, 0xD4, 0x9E, 0x91, 0x9C, 0xC2, 0xF0, 0xB8, 0x5A,  \
		0x0C

#define MAX_CONNECT 3
static uint8_t active_connections = 0;

static able_state_t global_state = ABLE_IDLE;
static uint16_t conn_handle_list[MAX_CONNECT];

uint8_t ble_addr_type;
static int ble_app_advertise(void);

static uint16_t read_notify_handle;
static uint16_t bat_handle;
static uint16_t write_handle;

static int current_battery_level = 100;

// Write data to ESP32 defined as server
static int device_write(uint16_t conn_handle, uint16_t attr_handle,
			struct ble_gatt_access_ctxt *ctxt, void *arg)
{
	ESP_LOGI(LOG_TAG, "Data from the client: %.*s\n", ctxt->om->om_len, ctxt->om->om_data);

	struct os_mbuf *om;
	char *data = "Notify Text";
	om = ble_hs_mbuf_from_flat(data, strlen(data));
	ble_gatts_notify_custom(conn_handle, read_notify_handle, om);
	return 0;
}
// Read data from ESP32 defined as server
static int device_read(uint16_t con_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
		       void *arg)
{
	// struct os_mbuf *om;
	char *data = "New Data on read request";
	// om = ble_hs_mbuf_from_flat(data, strlen(data));
	os_mbuf_append(ctxt->om, data, strlen(data));
	// ble_gattc_write_no_rsp_flat(con_handle, read_handle, data, strlen(data));
	return 0;
}

/* UUID string: FB8D2CC1-E0B3-4DD4-9E91-9AC2F0B85A0C */
// int uuid_byte_array[16u] = {0xFB, 0x8D, 0x2C, 0xC1, 0xE0, 0xB3, 0x4D, 0xD4,
// 			    0x9E, 0x91, 0x9A, 0xC2, 0xF0, 0xB8, 0x5A, 0x0C};
// ble_uuid128_t able_uuid = {
// 	.u = BLE_UUID_TYPE_128,
// 	.value = {0xFB, 0x8D, 0x2C, 0xC1, 0xE0, 0xB3, 0x4D, 0xD4, 0x9E, 0x91, 0x9A, 0xC2, 0xF0,
// 		  0xB8, 0x5A, 0x0C},
// };

// int ti_reverse_uuid[16u] = {0x0C, 0x5A, 0xB8, 0xF0, 0xC2, 0x9A, 0x91, 0x9E,
// 			    0xD4, 0x4D, 0xB3, 0xE0, 0xC1, 0x2C, 0x8D, 0xFB};

// Array of pointers to other service definitions
// UUID - Universal Unique Identifier

static int bat_read(uint16_t con_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
		    void *arg)
{
	// os_mbuf_append(ctxt->om, "12", strlen("12"));

	return 0;
}

void able_set_bat_level(uint8_t level)
{
	// int res = ble_gap_security_initiate(glob_conn_handle);
	current_battery_level = level;
	for (int i = 0; i < active_connections; ++i)
	{
		ESP_LOGW(LOG_TAG, "Bat level to %d", i);
		struct os_mbuf *om;
		uint8_t *data = (uint8_t *)&current_battery_level;
		om = ble_hs_mbuf_from_flat(data, 1);
		ble_gatts_indicate_custom(conn_handle_list[i], bat_handle, om);
	}
}

static const struct ble_gatt_svc_def gatt_svcs[] = {
	{
		.type = BLE_GATT_SVC_TYPE_PRIMARY,
		.uuid = BLE_UUID16_DECLARE(BATTERY_SVC_UUID),
		.characteristics =
			(struct ble_gatt_chr_def[]){
				{
					.uuid = BLE_UUID16_DECLARE(BATTERY_LEVEL_ATTR_UUID),
					.flags = BLE_GATT_CHR_F_INDICATE,
					.val_handle = &bat_handle,
					.access_cb = bat_read,
				},

				{0}},
	},
	{.type = BLE_GATT_SVC_TYPE_PRIMARY,
	 .uuid = BLE_UUID128_DECLARE(APP_SVC_UUID), // Define UUID for device type
	 .characteristics =
		 (struct ble_gatt_chr_def[]){
			 {
				 .uuid = BLE_UUID128_DECLARE(
					 APP_ATTR_READ_UUID), // Define UUID for reading
				 .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
				 .val_handle = &read_notify_handle,
				 .access_cb = device_read,
			 },
			 {
				 .uuid = BLE_UUID128_DECLARE(
					 APP_ATTR_WRITE_UUID), // Define UUID for writing
				 .flags = BLE_GATT_CHR_F_WRITE,
				 .access_cb = device_write,
				 .val_handle = &write_handle,
			 },

			 {0}}},
	{0},
};

static void delete_connection(uint16_t handle)
{
	// uint16_t tmp[MAX_CONNECT];
	ESP_LOGD(LOG_TAG, "Deleting handle (%d)", handle);
	int j = 0;
	for (int i = 0; i < MAX_CONNECT; ++i)
	{
		if (conn_handle_list[i] != handle)
		{
			conn_handle_list[j] = handle;
			++j;
			--active_connections;
		}
	}
}

// BLE event handling
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
	switch (event->type)
	{
	case BLE_GAP_EVENT_CONNECT:
		ESP_LOGI(LOG_TAG, "BLE GAP EVENT CONNECT %s",
			 event->connect.status == 0 ? "OK!" : "FAILED!");
		if (event->connect.status != 0)
		{
			ESP_LOGI(LOG_TAG, "Connect feiled with status %d", event->connect.status);
		}
		else
		{
			conn_handle_list[active_connections] = event->connect.conn_handle;
			ESP_LOGW(LOG_TAG, "New connection #%d (%d)", active_connections,
				 conn_handle_list[active_connections]);
			++active_connections;
			global_state = ABLE_CONNECTED;
		}
		break;
	case BLE_GAP_EVENT_DISCONNECT:
		ESP_LOGI(LOG_TAG, "BLE GAP EVENT DISCONNECTED");
		global_state = ABLE_DISCONNECTED;
		delete_connection(event->connect.conn_handle);
		break;
	case BLE_GAP_EVENT_ADV_COMPLETE:
		ESP_LOGI(LOG_TAG, "BLE GAP EVENT BLE_GAP_EVENT_ADV_COMPLETE");
		break;

	default:
		ESP_LOGI(LOG_TAG, "EVENT: %d", event->type);
		break;
	}
	return 0;
}

// Define the BLE connection
static int ble_app_advertise(void)
{
	esp_err_t ret;
	// GAP - device name definition
	struct ble_hs_adv_fields fields;
	const char *device_name;
	memset(&fields, 0, sizeof(fields));
	device_name = ble_svc_gap_device_name(); // Read the BLE device name
	fields.name = (uint8_t *)device_name;
	fields.name_len = strlen(device_name);
	fields.name_is_complete = 1;
	ble_gap_adv_set_fields(&fields);

	// GAP - device connectivity definition
	struct ble_gap_adv_params adv_params;
	memset(&adv_params, 0, sizeof(adv_params));
	adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
	adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
	ret = ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event,
				NULL);
	if (ret != 0)
	{
		ESP_LOGE(LOG_TAG, "ble_gap_adv_start error (%d)", ret);
		return ret;
	}
	return 0;
}

static void ble_app_on_sync(void)
{
	ble_hs_id_infer_auto(0, &ble_addr_type);
}

static void host_task(void *param)
{
	nimble_port_run();
}

int able_init(void)
{
	global_state = ABLE_IDLE;
	esp_log_level_set(LOG_TAG, LOG_LOCAL_LEVEL);
	esp_err_t ret;
	ret = nvs_flash_init();
	if (ret != 0)
	{
		ESP_LOGE(LOG_TAG, "nvs_flash_init Error.");
		ESP_ERROR_CHECK(ret);
		return -1;
	}
	// esp_nimble_hci_and_controller_init();      // 2 - Initialize ESP controller
	ret = nimble_port_init(); // 3 - Initialize the host stack
	if (ret != ESP_OK)
	{
		ESP_LOGE(LOG_TAG, "Failed to init nimble %d ", ret);
		return ret;
	}

	ble_svc_gap_device_name_set(
		"BLE-Server"); // 4 - Initialize NimBLE configuration - server name

	ble_svc_gap_init();  // 4 - Initialize NimBLE configuration - gap service
	ble_svc_gatt_init(); // 4 - Initialize NimBLE configuration - gatt service
	ret = ble_gatts_count_cfg(
		gatt_svcs); // 4 - Initialize NimBLE configuration - config gatt services
	if (ret != 0)
	{
		ESP_LOGE(LOG_TAG, "ble_gatts_count_cfg Error.");
		return -1;
	}
	ret = ble_gatts_add_svcs(
		gatt_svcs); // 4 - Initialize NimBLE configuration - queues gatt services.
	if (ret != 0)
	{
		ESP_LOGE(LOG_TAG, "ble_gatts_add_svcs Error.");
		return -1;
	}
	ble_hs_cfg.sync_cb = ble_app_on_sync; // 5 - Initialize application
	nimble_port_freertos_init(host_task); // 6 - Run the thread
	global_state = ABLE_DISCONNECTED;
	return 0;
}

int able_start_advertising()
{
	if (active_connections >= MAX_CONNECT)
	{
		ESP_LOGE(LOG_TAG, "MAX Connection limit (%d).", MAX_CONNECT);
		return -1;
	}
	int ret = ble_app_advertise();
	if (ret != 0)
	{
		if (active_connections == 0)
		{
			global_state = ABLE_DISCONNECTED;
		}
		return ret;
	}
	global_state = ABLE_ADV;
	return 0;
}

void able_stop_advertising()
{
	if (global_state != ABLE_ADV)
	{
		ESP_LOGW(LOG_TAG, "Can't stop adv. Not advertising.");
		return;
	}
	int ret = ble_gap_adv_stop();
	if (ret != 0)
	{
		ESP_LOGE(LOG_TAG, "ble_gap_adv_stop error (%d)", ret);
		return;
	}
	global_state = ABLE_DISCONNECTED;
}

void able_write_all(char *data, size_t data_len)
{
	struct os_mbuf *om;
	om = ble_hs_mbuf_from_flat(data, data_len);
	for (int i = 0; i < active_connections; ++i)
	{
		ESP_LOGW(LOG_TAG, "Writing to %d", i);
		ble_gattc_write(conn_handle_list[i], write_handle, om, NULL, NULL);
	}
}

void able_notify_all(char *data, size_t data_len)
{
	for (int i = 0; i < active_connections; ++i)
	{
		struct os_mbuf *om;
		om = ble_hs_mbuf_from_flat(data, data_len);
		ESP_LOGW(LOG_TAG, "Notify to %d", i);
		ble_gatts_notify_custom(conn_handle_list[i], read_notify_handle, om);
	}
}

able_state_t able_get_state()
{
	return global_state;
}

void able_clear_bonded(void)
{
	int ret = ble_store_clear();
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "Error deleting bonded devices.");
		return;
	}
}

uint8_t able_get_active_connection_count()
{
	return active_connections;
}
