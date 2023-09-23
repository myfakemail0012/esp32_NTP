#include <stdio.h>
#include <stdlib.h>

#include "time.h"

#define SHOW_TASKS 1
#define SHOW_TIME  1

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#ifndef LOG_TAG
#define LOG_TAG "MAIN"
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_err.h"

#include "sdkconfig.h"

#include "app_wifi.h"
#include "app_sntp.h"

#include "driver/gpio.h"

#include "button_events.h"

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
	// .btn_events_released_cb = button_release_event,
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
}

void button_release_event(int button)
{
	ESP_LOGI(LOG_TAG, "Release event (btn: %d)", button);
}

void button_click_event(int button)
{
	ESP_LOGI(LOG_TAG, "Click event (btn: %d)", button);
}

void gpio_init(uint64_t mask)
{
	gpio_config_t io_conf;
	// interrupt of rising edge
	io_conf.intr_type = GPIO_INTR_DISABLE; // GPIO_INTR_POSEDGE;
	// bit mask of the pins, use GPIO4/5 here
	io_conf.pin_bit_mask = mask;
	// set as input mode
	io_conf.mode = GPIO_MODE_INPUT;
	// enable pull-up mode
	io_conf.pull_up_en = 1;
	io_conf.pull_down_en = 0;
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

void app_main(void)
{
	esp_log_level_set(LOG_TAG, LOG_LOCAL_LEVEL);
	ESP_LOGD(LOG_TAG, "Starting...");
	gpio_init(BTN_SEL);
	// gpio_init(BTN2_GPIO_SEL);
	printf("Hello\n");

	ESP_LOGD(LOG_TAG, "wifi_init");
	wifi_init(got_ip_cb);
	sntp_start(time_sync_notification_cb);
	vTaskDelay(10);
	ESP_LOGD(LOG_TAG, "wifi_connection");
	wifi_connect();

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

	btn_event_start_task(&button1);
	btn_event_start_task(&button2);
	get_tasks();

	while (1)
	{
		vTaskDelay(100);
	}
}
