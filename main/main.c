#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "time.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_err.h"

#include "driver/gpio.h"
#include "sdkconfig.h"

#if CONFIG_WIFI
#include "app_wifi.h"
#endif
#if CONFIG_SNTP
#include "app_sntp.h"
#endif

#include "app_ble.h"

// #include "app_spi.h"

// #include "app_rtc.h"

#include "button_events.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#ifndef LOG_TAG
#define LOG_TAG "MAIN"
#endif

#define SHOW_TASKS 0
#define SHOW_TIME  1

#define BTN1_GPIO     GPIO_NUM_0
#define BTN1_GPIO_SEL (1ULL << BTN1_GPIO)

#define BTN2_GPIO     GPIO_NUM_23
#define BTN2_GPIO_SEL (1ULL << BTN2_GPIO)

#define BTN_SEL (BTN1_GPIO_SEL | BTN2_GPIO_SEL)

typedef enum
{
	BUTTON1 = 1,
	BUTTON2,
} e_buttons_t;

void button_press_event(int button);
void button_hold_event(int button);
void button_release_event(int button);
void button_click_event(int button);

button_events_t button1 = {
	.pin = BTN1_GPIO,
	.button = BUTTON1,
	.btn_events_click_cb = button_click_event,
	.btn_events_hold_cb = button_hold_event,
	.btn_events_pressed_cb = button_press_event,
	.btn_events_released_cb = button_release_event,
};

button_events_t button2 = {
	.pin = BTN2_GPIO,
	.button = BUTTON2,
	.btn_events_click_cb = button_click_event,
	.btn_events_hold_cb = button_hold_event,
	.btn_events_pressed_cb = button_press_event,
	.btn_events_released_cb = button_release_event,
	.hold_time = 2000000,
};

void button_press_event(int button)
{
	ESP_LOGI(LOG_TAG, "Press event (btn: %d)", button);
}

void button_hold_event(int button)
{
	ESP_LOGI(LOG_TAG, "Hold event (btn: %d)", button);
	if (button == BUTTON1)
	{
		able_start_advertising();
	}
}

void button_release_event(int button)
{
	ESP_LOGI(LOG_TAG, "Release event (btn: %d)", button);
}

uint8_t lvl = 100;
void button_click_event(int button)
{
	ESP_LOGI(LOG_TAG, "Click event (btn: %d)", button);
	if (button == BUTTON1)
	{
		lvl -= 5;
		// char *txt = "Sending new data via notify message";
		// able_notify_all(txt, strlen(txt));
		if (lvl <= 0 || lvl > 100)
		{
			lvl = 100;
		}
		able_set_bat_level(lvl);
	}
	if (button == BUTTON2)
	{
		able_clear_bonded();
	}
}

void gpio_init(uint64_t mask)
{
	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_INTR_DISABLE; // GPIO_INTR_POSEDGE;
	io_conf.pin_bit_mask = mask;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_up_en = 1;
	io_conf.pull_down_en = 0;
	gpio_config(&io_conf);
}

#ifdef CONFIG_SNTP
void time_sync_notification_cb()
{
	ESP_LOGI(LOG_TAG, "Notification of a time synchronization event");
}
#endif

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

#ifdef CONFIG_WIFI
void got_ip_cb(void)
{
	ESP_LOGD(LOG_TAG, "Got IP...");
	print_current_time();
	sync_time();
	print_current_time();
}
#endif

#if SHOW_TASKS == 1
void get_tasks()
{
	char raw[256];
	vTaskList(raw);
	printf("%s", raw);
}
#endif

// void get_seconds(void *param)
// {
// 	while (true)
// 	{
// 		uint8_t sec = artc_read_seconds();
// 		ESP_LOGW(LOG_TAG, "Current second: %d", sec);
// 		vTaskDelay(1000 / portTICK_PERIOD_MS);
// 	}
// }

void app_main(void)
{
	esp_log_level_set(LOG_TAG, LOG_LOCAL_LEVEL);
	ESP_LOGD(LOG_TAG, "Starting...");
	gpio_init(BTN_SEL);

	printf("Hello\n");

#ifdef CONFIG_WIFI
	ESP_LOGD(LOG_TAG, "wifi_init");
	wifi_init(got_ip_cb);
	sntp_start(time_sync_notification_cb);
	vTaskDelay(10);
	ESP_LOGD(LOG_TAG, "wifi_connection");
	wifi_connect();
#endif

	if (btn_events_init(&button1) != 0)
	{
		ESP_LOGE(LOG_TAG, "Can't init button.");
		while (1)
			;
	}
	if (btn_events_init(&button2) != 0)
	{
		ESP_LOGE(LOG_TAG, "Can't init button.");
		while (1)
			;
	}

#if SHOW_TASKS == 1
	get_tasks();
#endif

	able_init();
	able_set_bat_level(lvl);
	if (is_btn_pressed(&button1))
	{
		ESP_LOGD(LOG_TAG, "Deleting all bonded devices");
		able_clear_bonded();
		while (is_btn_pressed(&button1))
		{
			vTaskDelay(10);
		}
	}

	btn_event_start_task(&button1);
	btn_event_start_task(&button2);
	// aspi_init();
	// artc_init();

	// xTaskCreate(get_seconds, "RTC_READ", 3000, NULL, tskIDLE_PRIORITY, NULL);

	while (1)
	{
		vTaskDelay(10);
	}
}
