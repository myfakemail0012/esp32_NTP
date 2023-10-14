#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_device.h"

#include "app_ble.h"

#define LOG_LOCAL_LEVEL CONFIG_ABLE_LOG_LEVEL
#include "esp_log.h"
#ifndef LOG_TAG
#define LOG_TAG "ABLE"
#endif

#define ESP_APP_ID 0x55

#define ADV_CONFIG_FLAG      (1 << 0)
#define SCAN_RSP_CONFIG_FLAG (1 << 1)

/// Attributes State Machine
enum BAS_e
{
	BAS_IDX_SVC,

	BAS_IDX_BATT_LVL_CHAR,
	BAS_IDX_BATT_LVL_VAL,
	BAS_IDX_BATT_LVL_NTF_CFG,
	BAS_IDX_BATT_LVL_PRES_FMT,

	BAS_IDX_NB,
};
enum CUSTOM_e
{
	CUS_IDX_SVC,
	CUS_IDX_BATT_LVL_CHAR,
	CUS_IDX_VAL,
	CUS_IDX_BATT_LVL_NTF_CFG,
	CUS_IDX_BATT_LVL_PRES_FMT,

	CUS_IDX_NB,
};

// static uint16_t handle_table[BAS_IDX_NB];
static void gatts_bas_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
					    esp_ble_gatts_cb_param_t *param);
typedef struct
{
	uint16_t handles[BAS_IDX_NB];
} att_info_t;

typedef struct
{
	uint16_t conn_ids[CONFIG_APP_BLE_MAX_CONNECTIONS];
	uint8_t conn_count;
	uint16_t gatts_if;
	esp_gatts_cb_t gatts_cb;
} gatt_info_t;

static gatt_info_t gatt_info = {
	.gatts_if = ESP_GATT_IF_NONE,
	.gatts_cb = gatts_bas_profile_event_handler,
};

att_info_t bas_info;
att_info_t cus_info;

// static uint16_t battery_notify_handle = 0;

static uint8_t adv_config_done = 0;

static esp_ble_adv_params_t adv_params = {
	.adv_int_min = 0x100,
	.adv_int_max = 0x100,
	.adv_type = ADV_TYPE_IND,
	.own_addr_type = BLE_ADDR_TYPE_PUBLIC,
	.channel_map = ADV_CHNL_ALL,
	.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};
static uint8_t sec_service_uuid[16] = {
	/* LSB <-------------------------------------------------------------------------------->
	   MSB */
	// first uuid, 16bit, [12],[13] is the value
	0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
	0x00, 0x10, 0x00, 0x00, 0x18, 0x0D, 0x00, 0x00,
};

// config adv data
static esp_ble_adv_data_t able_adv_config = {
	.set_scan_rsp = true,
	.include_txpower = true,
	.min_interval = 0x0006, // slave connection min interval, Time = min_interval * 1.25 msec
	.max_interval = 0x0010, // slave connection max interval, Time = max_interval * 1.25 msec
	.appearance = 0x00,
	.manufacturer_len = 0,       // TEST_MANUFACTURER_DATA_LEN,
	.p_manufacturer_data = NULL, //&test_manufacturer[0],
	.service_data_len = 0,
	.p_service_data = NULL,
	.service_uuid_len = sizeof(sec_service_uuid),
	.p_service_uuid = sec_service_uuid,
	.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// config scan response data
static uint8_t test_manufacturer[] = "MAD PRODUCTION";
static esp_ble_adv_data_t able_scan_rsp_config = {
	.set_scan_rsp = true,
	.include_name = true,
	.manufacturer_len = sizeof(test_manufacturer),
	.p_manufacturer_data = test_manufacturer,
};

static void gatts_bas_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
					    esp_ble_gatts_cb_param_t *param);
static void able_battery_indicate(void);

// struct gatts_profile_inst
// {
// 	esp_gatts_cb_t gatts_cb;
// 	uint16_t gatts_if;
// 	uint16_t app_id;
// 	uint16_t conn_id;
// 	uint16_t service_handle;
// 	esp_gatt_srvc_id_t service_id;
// 	uint16_t char_handle;
// 	esp_bt_uuid_t char_uuid;
// 	esp_gatt_perm_t perm;
// 	esp_gatt_char_prop_t property;
// 	uint16_t descr_handle;
// 	esp_bt_uuid_t descr_uuid;
// };

// static struct gatts_profile_inst able_profile_tab[PROFILE_NUM] = {
// 	[PROFILE_APP_IDX] =
// 		{
// 			.gatts_cb = gatts_profile_event_handler,
// 			.gatts_if = ESP_GATT_IF_NONE, /* Not get the gatt_if, so initial is
// 							 ESP_GATT_IF_NONE */
// 		},

// };

static const uint16_t battery_svc = ESP_GATT_UUID_BATTERY_SERVICE_SVC;

#define CHAR_DECLARATION_SIZE (sizeof(uint8_t))
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

// static const uint8_t char_prop_notify = ESP_GATT_CHAR_PROP_BIT_NOTIFY;
// static const uint8_t char_prop_read = ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t char_prop_read_notify =
	ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
// static const uint8_t char_prop_read_write =
// 	ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ;
// static const uint8_t char_prop_read_indicate =
// 	ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_INDICATE;
static const uint8_t char_prop_read_write_indicate = ESP_GATT_CHAR_PROP_BIT_WRITE |
						     ESP_GATT_CHAR_PROP_BIT_READ |
						     ESP_GATT_CHAR_PROP_BIT_INDICATE;

static const uint16_t battery_level_uuid = ESP_GATT_UUID_BATTERY_LEVEL;

static const uint16_t cus_svc = 0xFFEA;
static const uint16_t cus_val_uuid = 0xDEAD;
static char cus_val_buf[] = "My string data";
static const uint8_t cus_lev_ccc[2] = {0x00, 0x00};

static const uint16_t char_format_uuid = ESP_GATT_UUID_CHAR_PRESENT_FORMAT;

static const uint8_t bat_lev_ccc[2] = {0x00, 0x00};

/// characteristic presentation information
struct prf_char_pres_fmt
{
	/// Unit (The Unit is a UUID)
	uint16_t unit;
	/// Description
	uint16_t description;
	/// Format
	uint8_t format;
	/// Exponent
	uint8_t exponent;
	/// Name space
	uint8_t name_space;
};

struct prf_char_pres_fmt my_format = {
	.unit = 0xDADA, .description = 0x0123, .format = 0x19, .name_space = 1,
	// .exponent = 0,
};

static uint8_t battery_lev = 45;

static const esp_gatts_attr_db_t bas_att_db[BAS_IDX_NB] = {
	// Battary Service Declaration
	[BAS_IDX_SVC] = {{ESP_GATT_AUTO_RSP},
			 {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
			  sizeof(uint16_t), sizeof(battery_svc), (uint8_t *)&battery_svc}},

	// Battary level Characteristic Declaration
	[BAS_IDX_BATT_LVL_CHAR] = {{ESP_GATT_AUTO_RSP},
				   {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid,
				    ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE,
				    CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_notify}},

	// Battary level Characteristic Value
	[BAS_IDX_BATT_LVL_VAL] = {{ESP_GATT_AUTO_RSP},
				  {ESP_UUID_LEN_16, (uint8_t *)&battery_level_uuid,
				   ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(uint8_t),
				   &battery_lev}},

	// Battary level Characteristic - Client Characteristic Configuration Descriptor
	[BAS_IDX_BATT_LVL_NTF_CFG] = {{ESP_GATT_AUTO_RSP},
				      {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid,
				       ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(uint16_t),
				       sizeof(bat_lev_ccc), (uint8_t *)bat_lev_ccc}},

	// Battary level report Characteristic Declaration
	[BAS_IDX_BATT_LVL_PRES_FMT] = {{ESP_GATT_AUTO_RSP},
				       {ESP_UUID_LEN_16, (uint8_t *)&char_format_uuid,
					ESP_GATT_PERM_READ, sizeof(struct prf_char_pres_fmt), 0,
					NULL}},
};
static const esp_gatts_attr_db_t cus_att_db[CUS_IDX_NB] = {
	// Battary Service Declaration
	// Custom Service Declaration
	[CUS_IDX_SVC] = {{ESP_GATT_AUTO_RSP},
			 {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
			  sizeof(uint16_t), sizeof(cus_svc), (uint8_t *)&cus_svc}},

	// Custom level Characteristic Declaration
	[CUS_IDX_BATT_LVL_CHAR] = {{ESP_GATT_AUTO_RSP},
				   {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid,
				    ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, CHAR_DECLARATION_SIZE,
				    CHAR_DECLARATION_SIZE,
				    (uint8_t *)&char_prop_read_write_indicate}},

	// Custom level Characteristic Value
	[CUS_IDX_VAL] = {{ESP_GATT_AUTO_RSP},
			 {ESP_UUID_LEN_16, (uint8_t *)&cus_val_uuid,
			  ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 15, 15,
			  (uint8_t *)cus_val_buf}},

	// Custom level Characteristic - Client Characteristic Configuration Descriptor
	[CUS_IDX_BATT_LVL_NTF_CFG] = {{ESP_GATT_AUTO_RSP},
				      {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid,
				       ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(uint16_t),
				       sizeof(cus_lev_ccc), (uint8_t *)cus_lev_ccc}},

	// Custom level report Characteristic Declaration
	[CUS_IDX_BATT_LVL_PRES_FMT] = {{ESP_GATT_AUTO_RSP},
				       {ESP_UUID_LEN_16, (uint8_t *)&char_format_uuid,
					ESP_GATT_PERM_READ, 7, 7, (uint8_t *)&my_format}},
};

static void remove_conn_id(uint16_t id)
{
	if (gatt_info.conn_count == 0)
	{

		ESP_LOGE(LOG_TAG, "ERROR! No connections. Can't delete.");
		return;
	}
	ESP_LOGD(LOG_TAG, "Removing connection (id: %d)", id);
	int j = 0;
	for (int i = 0; i < gatt_info.conn_count; ++i)
	{
		if (gatt_info.conn_ids[i] != id)
		{
			gatt_info.conn_ids[j++] = gatt_info.conn_ids[i];
		}
	}
	gatt_info.conn_count = j;
}

static void add_conn_id(uint16_t id)
{
	if (gatt_info.conn_count >= CONFIG_APP_BLE_MAX_CONNECTIONS)
	{
		ESP_LOGE(LOG_TAG, "ERROR! Trying to add connection when maximum number of "
				  "connection reached");
		return;
	}
	ESP_LOGD(LOG_TAG, "Adding connection (id: %d)", id);
	gatt_info.conn_ids[gatt_info.conn_count++] = id;
}

static char *esp_key_type_to_str(esp_ble_key_type_t key_type)
{
	char *key_str = NULL;
	switch (key_type)
	{
	case ESP_LE_KEY_NONE:
		key_str = "ESP_LE_KEY_NONE";
		break;
	case ESP_LE_KEY_PENC:
		key_str = "ESP_LE_KEY_PENC";
		break;
	case ESP_LE_KEY_PID:
		key_str = "ESP_LE_KEY_PID";
		break;
	case ESP_LE_KEY_PCSRK:
		key_str = "ESP_LE_KEY_PCSRK";
		break;
	case ESP_LE_KEY_PLK:
		key_str = "ESP_LE_KEY_PLK";
		break;
	case ESP_LE_KEY_LLK:
		key_str = "ESP_LE_KEY_LLK";
		break;
	case ESP_LE_KEY_LENC:
		key_str = "ESP_LE_KEY_LENC";
		break;
	case ESP_LE_KEY_LID:
		key_str = "ESP_LE_KEY_LID";
		break;
	case ESP_LE_KEY_LCSRK:
		key_str = "ESP_LE_KEY_LCSRK";
		break;
	default:
		key_str = "INVALID BLE KEY TYPE";
		break;
	}

	return key_str;
}

static void gatts_bas_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
					    esp_ble_gatts_cb_param_t *param)
{
	ESP_LOGD(LOG_TAG, "event = %x\n", event);
	switch (event)
	{
	case ESP_GATTS_REG_EVT:
		// generate a resolvable random address
		ESP_LOGD(LOG_TAG, "gatts_profile_event_handler ESP_GATTS_REG_EVT");

		esp_ble_gap_config_local_privacy(true);
		break;
	case ESP_GATTS_READ_EVT:
		break;
	case ESP_GATTS_WRITE_EVT:
		ESP_LOGD(LOG_TAG, "ESP_GATTS_WRITE_EVT, write value:");
		esp_log_buffer_hex(LOG_TAG, param->write.value, param->write.len);
		break;
	case ESP_GATTS_EXEC_WRITE_EVT:
		break;
	case ESP_GATTS_MTU_EVT:
		break;
	case ESP_GATTS_CONF_EVT:
		break;
	case ESP_GATTS_UNREG_EVT:
		break;
	case ESP_GATTS_DELETE_EVT:
		break;
	case ESP_GATTS_START_EVT:
		break;
	case ESP_GATTS_STOP_EVT:
		break;
	case ESP_GATTS_CONNECT_EVT:
		ESP_LOGD(LOG_TAG, "ESP_GATTS_CONNECT_EVT");
		/* start security connect with peer device when receive the connect event sent by
		 * the master */
		esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);

		// able_profile_tab[PROFILE_APP_IDX].conn_id = param->connect.conn_id;
		add_conn_id(param->connect.conn_id);
		break;
	case ESP_GATTS_DISCONNECT_EVT:
		ESP_LOGD(LOG_TAG, "ESP_GATTS_DISCONNECT_EVT, disconnect reason 0x%x",
			 param->disconnect.reason);
		remove_conn_id(param->connect.conn_id);
		/* start advertising again when missing the connect */
		// esp_ble_gap_start_advertising(&adv_params);
		break;
	case ESP_GATTS_OPEN_EVT:
		break;
	case ESP_GATTS_CANCEL_OPEN_EVT:
		break;
	case ESP_GATTS_CLOSE_EVT:
		break;
	case ESP_GATTS_LISTEN_EVT:
		break;
	case ESP_GATTS_CONGEST_EVT:
		break;
	case ESP_GATTS_CREAT_ATTR_TAB_EVT:
	{
		ESP_LOGD(LOG_TAG,
			 "ESP_GATTS_CREAT_ATTR_TAB_EVT The number handle = %x , svc_ins_id: %d",
			 param->add_attr_tab.num_handle, param->add_attr_tab.svc_inst_id);
		if (param->create.status == ESP_GATT_OK)
		{
			if (param->add_attr_tab.svc_inst_id == 0)
			{
				if (param->add_attr_tab.num_handle == BAS_IDX_NB)
				{

					memcpy(bas_info.handles, param->add_attr_tab.handles,
					       sizeof(bas_info.handles));
					esp_ble_gatts_start_service(bas_info.handles[BAS_IDX_SVC]);
				}
				else
				{
					ESP_LOGE(LOG_TAG,
						 "Create attribute table abnormally, num_handle "
						 "(%d) "
						 "doesn't equal to BAS_IDX_SVC(%d)",
						 param->add_attr_tab.num_handle, BAS_IDX_NB);
				}
			}
			if (param->add_attr_tab.svc_inst_id == 1)
			{
				if (param->add_attr_tab.num_handle == CUS_IDX_NB)
				{
					memcpy(cus_info.handles, param->add_attr_tab.handles,
					       sizeof(cus_info.handles));
					esp_ble_gatts_start_service(cus_info.handles[CUS_IDX_SVC]);
				}
				else
				{
					ESP_LOGE(LOG_TAG,
						 "Create attribute table abnormally, num_handle "
						 "(%d) "
						 "doesn't equal to CUS_IDX_NB(%d)",
						 param->add_attr_tab.num_handle, BAS_IDX_NB);
				}
			}
		}
		else
		{
			ESP_LOGE(LOG_TAG, " Create attribute table failed, error code = %x",
				 param->create.status);
		}
		break;
	}

	default:
		break;
	}
}

static char *esp_auth_req_to_str(esp_ble_auth_req_t auth_req)
{
	char *auth_str = NULL;
	switch (auth_req)
	{
	case ESP_LE_AUTH_NO_BOND:
		auth_str = "ESP_LE_AUTH_NO_BOND";
		break;
	case ESP_LE_AUTH_BOND:
		auth_str = "ESP_LE_AUTH_BOND";
		break;
	case ESP_LE_AUTH_REQ_MITM:
		auth_str = "ESP_LE_AUTH_REQ_MITM";
		break;
	case ESP_LE_AUTH_REQ_BOND_MITM:
		auth_str = "ESP_LE_AUTH_REQ_BOND_MITM";
		break;
	case ESP_LE_AUTH_REQ_SC_ONLY:
		auth_str = "ESP_LE_AUTH_REQ_SC_ONLY";
		break;
	case ESP_LE_AUTH_REQ_SC_BOND:
		auth_str = "ESP_LE_AUTH_REQ_SC_BOND";
		break;
	case ESP_LE_AUTH_REQ_SC_MITM:
		auth_str = "ESP_LE_AUTH_REQ_SC_MITM";
		break;
	case ESP_LE_AUTH_REQ_SC_MITM_BOND:
		auth_str = "ESP_LE_AUTH_REQ_SC_MITM_BOND";
		break;
	default:
		auth_str = "INVALID BLE AUTH REQ";
		break;
	}

	return auth_str;
}

static void show_bonded_devices(void)
{
	int dev_num = esp_ble_get_bond_device_num();

	esp_ble_bond_dev_t *dev_list =
		(esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
	esp_ble_get_bond_device_list(&dev_num, dev_list);
	ESP_LOGD(LOG_TAG, "Bonded devices number : %d\n", dev_num);

	ESP_LOGD(LOG_TAG, "Bonded devices list : %d\n", dev_num);
	for (int i = 0; i < dev_num; i++)
	{
		esp_log_buffer_hex(LOG_TAG, (void *)dev_list[i].bd_addr, sizeof(esp_bd_addr_t));
	}

	free(dev_list);
}

void able_clear_bonded(void)
{
	int dev_num = esp_ble_get_bond_device_num();

	esp_ble_bond_dev_t *dev_list =
		(esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
	esp_ble_get_bond_device_list(&dev_num, dev_list);
	for (int i = 0; i < dev_num; i++)
	{
		esp_ble_remove_bond_device(dev_list[i].bd_addr);
	}

	free(dev_list);
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
				esp_ble_gatts_cb_param_t *param)
{
	/* If event is register event, store the gatts_if for each profile */

	ESP_LOGD(LOG_TAG, "gatts_event_handler. event: %d", event);
	if (event == ESP_GATTS_READ_EVT)
	{
		ESP_LOGD(LOG_TAG,
			 "ESP_GATTS_READ_EVT, status: %d,  attr_handle: %d, "
			 "conn_id: %d, gatts_if: %d\n",
			 param->conf.status, param->read.handle, param->read.conn_id, gatts_if);
	}
	if (event == ESP_GATTS_CREAT_ATTR_TAB_EVT)
	{
		ESP_LOGD(LOG_TAG,
			 "ESP_GATTS_CREAT_ATTR_TAB_EVT, status: %d,  attr_handle: %d, "
			 "service_handle: %d\n",
			 param->add_char.status, param->add_char.attr_handle,
			 param->add_char.service_handle);
	}

	if (event == ESP_GATTS_REG_EVT)
	{
		if (param->reg.status == ESP_GATT_OK)
		{
			ESP_LOGD(LOG_TAG, "ESP_GATTS_REG_EVT");
			esp_ble_gap_set_device_name(CONFIG_APP_BLE_DEVICE_NAME);

			esp_ble_gap_config_local_privacy(true);
			esp_err_t ret =
				esp_ble_gatts_create_attr_tab(bas_att_db, gatts_if, BAS_IDX_NB, 0);

			if (ret)
			{
				ESP_LOGE(LOG_TAG,
					 "gatts_event_handler(%d) %s enable controller failed: %s",
					 __LINE__, __func__, esp_err_to_name(ret));
				return;
			}
			gatt_info.gatts_if = gatts_if;

			ret = esp_ble_gatts_create_attr_tab(cus_att_db, gatts_if, CUS_IDX_NB, 1);
			if (ret)
			{
				ESP_LOGE(LOG_TAG,
					 "gatts_event_handler(%d) %s enable controller failed: %s",
					 __LINE__, __func__, esp_err_to_name(ret));
				return;
			}
		}
		else
		{
			ESP_LOGE(LOG_TAG, "Reg app failed, app_id %04x, status %d\n",
				 param->reg.app_id, param->reg.status);
			return;
		}
	}

	if (gatt_info.gatts_cb && gatts_if == ESP_GATT_IF_NONE)
	{
		gatt_info.gatts_cb(event, gatts_if, param);
	}
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
	ESP_LOGD(LOG_TAG, "GAP_EVT, event %d\n", event);

	switch (event)
	{
	case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
		ESP_LOGD(LOG_TAG, "ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT");
		adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
		// if (adv_config_done == 0)
		// {
		// 	esp_ble_gap_start_advertising(&heart_rate_adv_params);
		// }
		break;
	case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
		ESP_LOGD(LOG_TAG, "ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT");
		adv_config_done &= (~ADV_CONFIG_FLAG);
		// if (adv_config_done == 0)
		// {
		// 	esp_ble_gap_start_advertising(&heart_rate_adv_params);
		// }
		break;
	case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
		ESP_LOGD(LOG_TAG, "ESP_GAP_BLE_ADV_START_COMPLETE_EVT");
		// advertising start complete event to indicate advertising start successfully or
		// failed
		if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
		{
			ESP_LOGE(LOG_TAG, "advertising start failed, error status = %x",
				 param->adv_start_cmpl.status);
			break;
		}
		ESP_LOGI(LOG_TAG, "Advertising start success");
		break;
	case ESP_GAP_BLE_PASSKEY_REQ_EVT: /* passkey request event */
		ESP_LOGD(LOG_TAG, "ESP_GAP_BLE_PASSKEY_REQ_EVT");
		/* Call the following function to input the passkey which is displayed on the remote
		 * device */
		// esp_ble_passkey_reply(heart_rate_profile_tab[HEART_PROFILE_APP_IDX].remote_bda,
		// true, 0x00);
		break;
	case ESP_GAP_BLE_OOB_REQ_EVT:
	{
		ESP_LOGD(LOG_TAG, "ESP_GAP_BLE_OOB_REQ_EVT");
		uint8_t tk[16] = {1}; // If you paired with OOB, both devices need to use the same
				      // tk
		esp_ble_oob_req_reply(param->ble_security.ble_req.bd_addr, tk, sizeof(tk));
		break;
	}
	case ESP_GAP_BLE_LOCAL_IR_EVT: /* BLE local IR event */
		ESP_LOGD(LOG_TAG, "ESP_GAP_BLE_LOCAL_IR_EVT");
		break;
	case ESP_GAP_BLE_LOCAL_ER_EVT: /* BLE local ER event */
		ESP_LOGD(LOG_TAG, "ESP_GAP_BLE_LOCAL_ER_EVT");
		break;
	case ESP_GAP_BLE_NC_REQ_EVT:
		/* The app will receive this evt when the IO has DisplayYesNO capability and the
		peer device IO also has DisplayYesNo capability. show the passkey number to the user
		to confirm it with the number displayed by peer device. */
		esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, true);
		ESP_LOGD(LOG_TAG, "ESP_GAP_BLE_NC_REQ_EVT, the passkey Notify number:%" PRIu32,
			 param->ble_security.key_notif.passkey);
		break;
	case ESP_GAP_BLE_SEC_REQ_EVT:
		ESP_LOGD(LOG_TAG, "ESP_GAP_BLE_SEC_REQ_EVT");
		/* send the positive(true) security response to the peer device to accept the
		security request. If not accept the security request, should send the security
		response with negative(false) accept value*/
		esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
		break;
	case ESP_GAP_BLE_PASSKEY_NOTIF_EVT: /// the app will receive this evt when the IO  has
					    /// Output capability and the peer device IO has Input
					    /// capability.
		/// show the passkey number to the user to input it in the peer device.
		ESP_LOGD(LOG_TAG, "The passkey Notify number:%06" PRIu32,
			 param->ble_security.key_notif.passkey);
		break;
	case ESP_GAP_BLE_KEY_EVT:
		// shows the ble key info share with peer device to the user.
		ESP_LOGD(LOG_TAG, "key type = %s",
			 esp_key_type_to_str(param->ble_security.ble_key.key_type));
		break;
	case ESP_GAP_BLE_AUTH_CMPL_EVT:
	{
		esp_bd_addr_t bd_addr;
		memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
		ESP_LOGD(LOG_TAG, "remote BD_ADDR: %08x%04x",
			 (bd_addr[0] << 24) + (bd_addr[1] << 16) + (bd_addr[2] << 8) + bd_addr[3],
			 (bd_addr[4] << 8) + bd_addr[5]);
		ESP_LOGD(LOG_TAG, "address type = %d", param->ble_security.auth_cmpl.addr_type);
		ESP_LOGD(LOG_TAG, "pair status = %s",
			 param->ble_security.auth_cmpl.success ? "success" : "fail");
		if (!param->ble_security.auth_cmpl.success)
		{
			ESP_LOGE(LOG_TAG, "(%d) fail reason = 0x%x", __LINE__,
				 param->ble_security.auth_cmpl.fail_reason);
		}
		else
		{
			ESP_LOGD(LOG_TAG, "auth mode = %s",
				 esp_auth_req_to_str(param->ble_security.auth_cmpl.auth_mode));
		}
		show_bonded_devices();
		break;
	}
	case ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT:
	{
		ESP_LOGD(LOG_TAG, "ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT status = %d",
			 param->remove_bond_dev_cmpl.status);
		ESP_LOGD(LOG_TAG, "ESP_GAP_BLE_REMOVE_BOND_DEV");
		ESP_LOGD(LOG_TAG, "-----ESP_GAP_BLE_REMOVE_BOND_DEV----");
		esp_log_buffer_hex(LOG_TAG, (void *)param->remove_bond_dev_cmpl.bd_addr,
				   sizeof(esp_bd_addr_t));
		ESP_LOGD(LOG_TAG, "------------------------------------");
		break;
	}
	case ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT:
		ESP_LOGD(LOG_TAG, "ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT");
		if (param->local_privacy_cmpl.status != ESP_BT_STATUS_SUCCESS)
		{
			ESP_LOGE(LOG_TAG, "config local privacy failed, error status = %x",
				 param->local_privacy_cmpl.status);
			break;
		}

		esp_err_t ret = esp_ble_gap_config_adv_data(&able_adv_config);
		if (ret)
		{
			ESP_LOGE(LOG_TAG, "config adv data failed, error code = %x", ret);
		}
		else
		{
			adv_config_done |= ADV_CONFIG_FLAG;
		}

		ret = esp_ble_gap_config_adv_data(&able_scan_rsp_config);
		if (ret)
		{
			ESP_LOGE(LOG_TAG, "config adv data failed, error code = %x", ret);
		}
		else
		{
			adv_config_done |= SCAN_RSP_CONFIG_FLAG;
		}

		break;
	default:
		break;
	}
}

esp_err_t able_init(void)
{
	esp_log_level_set(LOG_TAG, LOG_LOCAL_LEVEL);
	esp_err_t ret;

	// Initialize NVS.
	ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	ret = esp_bt_controller_init(&bt_cfg);
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "%s init controller failed: %s", __func__, esp_err_to_name(ret));
		return ret;
	}
	ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "%s enable controller failed: %s", __func__,
			 esp_err_to_name(ret));
		return ret;
	}

	ESP_LOGI(LOG_TAG, "%s init bluetooth", __func__);
	ret = esp_bluedroid_init();
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
		return ret;
	}
	ret = esp_bluedroid_enable();
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
		return ret;
	}

	ret = esp_ble_gatts_register_callback(gatts_event_handler);
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "gatts register error, error code = %x", ret);
		return ret;
	}
	ret = esp_ble_gap_register_callback(gap_event_handler);
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "gap register error, error code = %x", ret);
		return ret;
	}

	// esp_ble_gatts_create_attr_tab(bas_att_db, gatts_if, BAS_IDX_NB, 0);

	ret = esp_ble_gatts_app_register(ESP_APP_ID);
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "gatts app register error, error code = %x", ret);
		return ret;
	}

	/* set the security iocap & auth_req & key size & init key response key parameters to the
	 * stack*/
	esp_ble_auth_req_t auth_req =
		ESP_LE_AUTH_REQ_SC_MITM_BOND;       // bonding with peer device after authentication
	esp_ble_io_cap_t iocap = ESP_IO_CAP_KBDISP; // ESP_IO_CAP_NONE; // set the IO capability to
						    // No output No input
	uint8_t key_size = 16;                      // the key size should be 7~16 bytes
	uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
	uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
	// set static passkey
	uint32_t passkey = 123456;
	uint8_t auth_option = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE;
	uint8_t oob_support = ESP_BLE_OOB_DISABLE;
	ret = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey,
					     sizeof(uint32_t));
	if (ret)
	{
		ESP_LOGE(LOG_TAG,
			 "esp_ble_gap_set_security_param error at line %d, error code = %x",
			 __LINE__, ret);
		return ret;
	}
	ret = esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req,
					     sizeof(uint8_t));
	if (ret)
	{
		ESP_LOGE(LOG_TAG,
			 "esp_ble_gap_set_security_param error at line %d, error code = %x",
			 __LINE__, ret);
		return ret;
	}
	ret = esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
	if (ret)
	{
		ESP_LOGE(LOG_TAG,
			 "esp_ble_gap_set_security_param error at line %d, error code = %x",
			 __LINE__, ret);
		return ret;
	}
	ret = esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
	if (ret)
	{
		ESP_LOGE(LOG_TAG,
			 "esp_ble_gap_set_security_param error at line %d, error code = %x",
			 __LINE__, ret);
		return ret;
	}
	ret = esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH,
					     &auth_option, sizeof(uint8_t));
	if (ret)
	{
		ESP_LOGE(LOG_TAG,
			 "esp_ble_gap_set_security_param error at line %d, error code = %x",
			 __LINE__, ret);
		return ret;
	}
	ret = esp_ble_gap_set_security_param(ESP_BLE_SM_OOB_SUPPORT, &oob_support, sizeof(uint8_t));
	if (ret)
	{
		ESP_LOGE(LOG_TAG,
			 "esp_ble_gap_set_security_param error at line %d, error code = %x",
			 __LINE__, ret);
		return ret;
	}
	/* If your BLE device acts as a Slave, the init_key means you hope which types of key of the
	master should distribute to you, and the response key means which key you can distribute to
	the master; If your BLE device acts as a master, the response key means you hope which types
	of key of the slave should distribute to you, and the init key means which key you can
	distribute to the slave. */
	// esp_ble_gap_set_security_param(ESP_BLE_SM_PASSKEY, &passkey, sizeof(passkey));
	ret = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
	if (ret)
	{
		ESP_LOGE(LOG_TAG,
			 "esp_ble_gap_set_security_param error at line %d, error code = %x",
			 __LINE__, ret);
		return ret;
	}
	ret = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));
	if (ret)
	{
		ESP_LOGE(LOG_TAG,
			 "esp_ble_gap_set_security_param error at line %d, error code = %x",
			 __LINE__, ret);
		return ret;
	}
}

static void able_battery_indicate(void)
{
	for (int i = 0; i < gatt_info.conn_count; ++i)
	{
		ESP_LOGD(LOG_TAG,
			 "Sending indicate battery level. gatts_if: %d, conn_id: %d, char_handle: "
			 "%d, "
			 "level: %d",
			 gatt_info.gatts_if, gatt_info.conn_ids[i],
			 bas_info.handles[BAS_IDX_BATT_LVL_VAL], battery_lev);
		esp_err_t ret =
			esp_ble_gatts_send_indicate(gatt_info.gatts_if, gatt_info.conn_ids[i],
						    bas_info.handles[BAS_IDX_BATT_LVL_VAL],
						    sizeof(battery_lev), &battery_lev, false);
		if (ret)
		{
			ESP_LOGE(LOG_TAG, "indicate sending error, error code = %x", ret);
			return;
		}
	}
}

int able_start_advertising()
{
	if (gatt_info.conn_count >= CONFIG_APP_BLE_MAX_CONNECTIONS)
	{
		ESP_LOGW(LOG_TAG,
			 "Can't start advertising. Maximum number of connections reached.");
		return -1;
	}

	return esp_ble_gap_start_advertising(&adv_params);
}

void able_stop_advertising()
{
	esp_ble_gap_stop_advertising();
}

void able_write_all(char *data, size_t data_len)
{
}
void able_notify_all(char *data, size_t data_len)
{
}

able_state_t able_get_state()
{
	return ABLE_IDLE;
}

void able_set_bat_level(uint8_t level)
{
	battery_lev = level;
	ESP_LOGD(LOG_TAG, "New battery level: %d", level);
	esp_ble_gatts_set_attr_value(bas_info.handles[BAS_IDX_BATT_LVL_VAL], sizeof(battery_lev),
				     &battery_lev);
	able_battery_indicate();
}

uint8_t able_get_active_connection_count()
{
	return gatt_info.conn_count;
}

// int able_get_power_level_from_enum(able_powers_e lev)
// {
// 	switch (lev)
// 	{
// 	case ABLE_PWR_LVL_N12:
// 		return -12;
// 		break;
// 	case ABLE_PWR_LVL_N9:
// 		return -9;
// 		break;
// 	case ABLE_PWR_LVL_N6:
// 		return -6;
// 		break;
// 	case ABLE_PWR_LVL_N3:
// 		return -3;
// 		break;
// 	case ABLE_PWR_LVL_N0:
// 		return 0;
// 		break;
// 	case ABLE_PWR_LVL_P3:
// 		return 3;
// 		break;
// 	case ABLE_PWR_LVL_P6:
// 		return 6;
// 		break;
// 	case ABLE_PWR_LVL_P9:
// 		return 9;
// 		break;

// 	default:
// 		return -127;
// 		break;
// 	}
// }

// int able_set_tx_power_level(able_powers_e min, able_powers_e max)
// {
// 	ESP_LOGD(LOG_TAG, "Set power level (min: %ddBm, max:%ddBm)",
// 		 able_get_power_level_from_enum(min), able_get_power_level_from_enum(max));
// 	return esp_bredr_tx_power_set(min, max);
// }
// int able_get_tx_power_level(able_powers_e *min, able_powers_e *max)
// {
// 	return esp_bredr_tx_power_get((esp_power_level_t *)min, (esp_power_level_t *)max);
// }
