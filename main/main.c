#include <stdio.h>
#include <stdlib.h>

#include "time.h"

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

#include "sdkconfig.h"

#include "app_wifi.h"
#include "app_sntp.h"

#include "driver/gpio.h"
#include "driver/gptimer.h"

#define GPIO_INPUT_IO_0    0
#define GPIO_INPUT_PIN_SEL (1ULL << GPIO_INPUT_IO_0)

#define BTN_PRESSED_LEVEL  0
#define BTN_RELEASED_LEVEL 1

typedef enum
{
	BUTTON1 = 1,
} e_buttons_t;

typedef struct
{
	int pin;
	e_buttons_t button;
} s_buttons_t;

const s_buttons_t button1 = {
	.pin = GPIO_INPUT_IO_0,
	.button = BUTTON1,
};

void button_press_event(e_buttons_t button)
{
	ESP_LOGD(LOG_TAG, "Press event");
}

void button_hold_event(e_buttons_t button)
{
	ESP_LOGD(LOG_TAG, "Hold event");
}

void button_release_event(e_buttons_t button)
{
	ESP_LOGD(LOG_TAG, "Release event");
}

void button_click_event(e_buttons_t button)
{
	ESP_LOGD(LOG_TAG, "Click event");
}

gptimer_handle_t gptimer = NULL;
void check_button(void *param)
{
	s_buttons_t *button = (s_buttons_t *)param;
	static int last_state = 1;

	int current_state = gpio_get_level(button->pin);
	uint64_t uptime = 0;
	uint64_t last_time = 0;
	ESP_ERROR_CHECK(gptimer_get_raw_count(gptimer, &last_time));

	const uint64_t hold_time = 1000000;

	static bool parsed = false;

	while (true)
	{
		current_state = gpio_get_level(button->pin);
		ESP_ERROR_CHECK(gptimer_get_raw_count(gptimer, &uptime));

		if (current_state == last_state && (uptime - last_time < hold_time || parsed))
		{
			;
		}
		if (current_state == last_state && current_state == BTN_PRESSED_LEVEL &&
		    (uptime - last_time >= hold_time && !parsed))
		{
			button_hold_event(button->button);
			parsed = true;
		}
		if (current_state != last_state)
		{
			switch (current_state)
			{
			case BTN_PRESSED_LEVEL:
				button_press_event(button->button);
				parsed = false;
				break;

			case BTN_RELEASED_LEVEL:
				button_release_event(button->button);
				if (!parsed)
				{
					button_click_event(button->button);
				}
				break;
			default:
				break;
			}
			last_state = current_state;
			last_time = uptime;
		}

		vTaskDelay(10 / portTICK_PERIOD_MS);
	}
}

void start_timer()
{
	gptimer_config_t timer_config = {
		.clk_src = GPTIMER_CLK_SRC_DEFAULT,
		.direction = GPTIMER_COUNT_UP,
		.resolution_hz = 1 * 1000 * 1000, // 1MHz, 1 tick = 1us
	};
	ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));
	ESP_ERROR_CHECK(gptimer_enable(gptimer));
	ESP_ERROR_CHECK(gptimer_start(gptimer));
}

void stop_timer()
{
	ESP_ERROR_CHECK(gptimer_stop(gptimer));
	ESP_ERROR_CHECK(gptimer_disable(gptimer));
}

void gpio_init()
{
	gpio_config_t io_conf;
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

void check_button_old(void *pvParameters)
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
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}

void start_check_gpio(void)
{
	TaskHandle_t xHandle = NULL;

	xTaskCreate(check_button, "Check button", 1058 * 2, (void *)&button1, tskIDLE_PRIORITY,
		    &xHandle);
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
	start_timer();

	start_check_gpio();
	while (1)
	{
		vTaskDelay(100);
	}
}
