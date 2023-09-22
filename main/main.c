#include <stdio.h>
#include <stdlib.h>

#include "time.h"
#include "sdkconfig.h"

#define SHOW_TASKS 0
#define SHOW_TIME  1

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#ifndef LOG_TAG
#define LOG_TAG "MAIN"
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_err.h"

#include "app_wifi.h"
#include "app_sntp.h"

#include "driver/gpio.h"
#include "freertos/portmacro.h"

#define GPIO_INPUT_IO_0    0
#define GPIO_INPUT_PIN_SEL (1ULL << GPIO_INPUT_IO_0)

void gpio_init()
{
	gpio_config_t io_conf = {};
	// interrupt of rising edge
	io_conf.intr_type = GPIO_INTR_DISABLE; // GPIO_INTR_POSEDGE;
	// bit mask of the pins, use GPIO4/5 here
	io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
	// set as input mode
	io_conf.mode = GPIO_MODE_INPUT;
	// enable pull-up mode
	io_conf.pull_up_en = 1;
	gpio_config(&io_conf);
}

void time_sync_notification_cb()
{
	ESP_LOGI(LOG_TAG, "Notification of a time synchronization event");
}

void print_current_time()
{
	time_t now;
	char strftime_buf[64];
	struct tm timeinfo;

	time(&now);
	// Set timezone
	setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
	tzset();

	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	ESP_LOGI(LOG_TAG, "The current date/time  is: %s", strftime_buf);
}

void got_ip_cb(void)
{
	ESP_LOGD(LOG_TAG, "Got IP...");
	print_current_time();
	sync_time();
	print_current_time();
}

#if SHOW_TASKS == 1
void get_tasks()
{
	char raw[256];
	vTaskList(raw);
	printf("%s", raw);
}
#endif

void check_button(void *pvParameters)
{
	static int oldlevel = 1;
	while (true)
	{
		int level = gpio_get_level(GPIO_INPUT_IO_0);
		if (oldlevel != level)
		{
			ESP_LOGW(LOG_TAG, "Level: %d", level);
#if SHOW_TASKS == 1
			get_tasks();
#endif
#if SHOW_TIME == 1
			print_current_time();
#endif
		}
		oldlevel = level;
		vTaskDelay(100 / portTICK_RATE_MS);
	}
}

void start_check_gpio(void)
{
	static uint8_t ucParameterToPass;
	TaskHandle_t xHandle = NULL;

	xTaskCreate(check_button, "Check button", configIDLE_TASK_STACK_SIZE * 2,
		    &ucParameterToPass, tskIDLE_PRIORITY, &xHandle);
	configASSERT(xHandle);

	// if (xHandle != NULL)
	// {
	// 	vTaskDelete(xHandle);
	// }
}

void app_main(void)
{
	esp_log_level_set(LOG_TAG, LOG_LOCAL_LEVEL);
	ESP_LOGD(LOG_TAG, "Starting...");
	gpio_init();
	printf("Hello\n");

	ESP_LOGD(LOG_TAG, "wifi_init");
	wifi_init(got_ip_cb);
	sntp_start(time_sync_notification_cb);
	vTaskDelay(10);
	ESP_LOGD(LOG_TAG, "wifi_connection");
	wifi_connect();

	start_check_gpio();
	while (1)
	{
		vTaskDelay(100);
	}
}
