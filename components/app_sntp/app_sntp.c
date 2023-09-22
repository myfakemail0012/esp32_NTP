#include "esp_sntp.h"
#include "sdkconfig.h"
#include "app_sntp.h"

#define LOG_LOCAL_LEVEL CONFIG_SNTP_LOG_LEVEL
#include "esp_log.h"
#ifndef SNTP_LOG_TAG
#define SNTP_LOG_TAG "SNTP"
#endif

static on_sntp_cb sntp_cb;

static void time_sync_notification_cb(struct timeval *tv)
{
	sntp_cb();
}

static void initialize_sntp(void)
{
	ESP_LOGD(SNTP_LOG_TAG, "Initializing SNTP");
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, "pool.ntp.org");
	sntp_set_time_sync_notification_cb(time_sync_notification_cb);
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
	sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
#endif
	sntp_init();
}

static void obtain_time(void)
{
	// ESP_ERROR_CHECK(nvs_flash_init());
	// ESP_ERROR_CHECK(esp_netif_init());
	// ESP_ERROR_CHECK(esp_event_loop_create_default());

	/**
	 * NTP server address could be aquired via DHCP,
	 * see LWIP_DHCP_GET_NTP_SRV menuconfig option
	 */
#ifdef LWIP_DHCP_GET_NTP_SRV
	sntp_servermode_dhcp(1);
#endif

	/* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
	 * Read "Establishing Wi-Fi or Ethernet Connection" section in
	 * examples/protocols/README.md for more information about this function.
	 */
	// ESP_ERROR_CHECK(example_connect());

	initialize_sntp();

	// wait for time to be set
	time_t now = 0;
	struct tm timeinfo = {0};
	int retry = 0;
	const int retry_count = 10;
	while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count)
	{
		ESP_LOGD(SNTP_LOG_TAG, "Waiting for system time to be set... (%d/%d)", retry,
			 retry_count);
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
	time(&now);
	localtime_r(&now, &timeinfo);
}

void sync_time()
{
	time_t now;
	struct tm timeinfo;
	time(&now);
	localtime_r(&now, &timeinfo);
	// Is time set? If not, tm_year will be (1970 - 1900).
	if (timeinfo.tm_year < (2016 - 1900))
	{
		ESP_LOGD(SNTP_LOG_TAG, "Time is not set yet. Getting time over NTP.");
		obtain_time();
		// update 'now' variable with current time
		time(&now);
	}
}

void sntp_start(on_sntp_cb cb)
{
	esp_log_level_set(SNTP_LOG_TAG, LOG_LOCAL_LEVEL);
	sntp_cb = cb;
}
