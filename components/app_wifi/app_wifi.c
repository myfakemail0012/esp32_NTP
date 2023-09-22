#include <stdio.h>

#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#define LOG_LOCAL_LEVEL CONFIG_WIFI_LOG_LEVEL
#include "esp_log.h"
#ifndef WIFI_LOG_TAG
#define WIFI_LOG_TAG "WIFI"
#endif

#include "app_wifi.h"

on_ip_cb app_got_ip_cb;

static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base,
			       int32_t event_id, void *event_data)
{
	static char retry_num = 0;
	switch (event_id)
	{
	case WIFI_EVENT_STA_START:
		ESP_LOGD(WIFI_LOG_TAG, "WIFI CONNECTING....\n");
		break;
	case WIFI_EVENT_STA_CONNECTED:
		ESP_LOGD(WIFI_LOG_TAG, "WiFi CONNECTED.\n");
		break;
	case WIFI_EVENT_STA_DISCONNECTED:
		ESP_LOGD(WIFI_LOG_TAG, "WiFi lost connection\n");
		if (retry_num < 5)
		{
			esp_wifi_connect();
			retry_num++;
			ESP_LOGD(WIFI_LOG_TAG, "Retrying to Connect...\n");
		}
		break;
	case IP_EVENT_STA_GOT_IP:
		ESP_LOGD(WIFI_LOG_TAG, "Wifi got IP.\n\n");

		if (app_got_ip_cb != NULL)
		{
			app_got_ip_cb();
		}

		break;
	default:
		break;
	}
}

void wifi_init(on_ip_cb ip_cb)
{
	esp_log_level_set(WIFI_LOG_TAG, LOG_LOCAL_LEVEL);
	app_got_ip_cb = ip_cb;

	esp_err_t res;

	ESP_LOGD(WIFI_LOG_TAG, "nvs_flash_init");
	res = nvs_flash_init();
	ESP_ERROR_CHECK(res);
	res = esp_netif_init();
	ESP_ERROR_CHECK(res);
	res = esp_event_loop_create_default();
	ESP_ERROR_CHECK(res);
	(void)esp_netif_create_default_wifi_sta();

	const wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
	wifi_config_t wifi_configuration = {
		.sta =
			{
				.ssid = WIFI_SSID,
				.password = WIFI_PASSWORD,
			},
	};
	ESP_LOGD(WIFI_LOG_TAG, "esp_wifi_init");
	res = esp_wifi_init(&wifi_initiation);
	ESP_ERROR_CHECK(res);
	ESP_LOGD(WIFI_LOG_TAG, "esp_event_handler_register ESP_EVENT_ANY_ID");
	res = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
	ESP_ERROR_CHECK(res);
	ESP_LOGD(WIFI_LOG_TAG, "esp_event_handler_register IP_EVENT_STA_GOT_IP");
	res = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
	ESP_ERROR_CHECK(res);

	ESP_LOGD(WIFI_LOG_TAG, "esp_wifi_set_config");
	res = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
	ESP_ERROR_CHECK(res);
}

void wifi_connect(void)
{
	esp_err_t res;

	// strcpy((char *)wifi_configuration.sta.ssid, WIFI_SSID);
	// strcpy((char *)wifi_configuration.sta.password, WIFI_PASSWORD);

	ESP_LOGI(WIFI_LOG_TAG, "esp_wifi_start");
	res = esp_wifi_start();
	ESP_ERROR_CHECK(res);

	res = esp_wifi_set_mode(WIFI_MODE_STA);
	ESP_ERROR_CHECK(res);
	res = esp_wifi_connect();
	// ESP_ERROR_CHECK(res);
	ESP_LOGI(WIFI_LOG_TAG, "wifi_init_softap finished. SSID:%s  password:%s", WIFI_SSID,
		 WIFI_PASSWORD);
}