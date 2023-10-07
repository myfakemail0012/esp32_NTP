#include <string.h>

#include "button_events.h"
#include "driver/gptimer.h"

#if CONFIG_BTN_ACTIVE_LOW_LEVEL
#define BTN_PRESSED_LEVEL  0
#define BTN_RELEASED_LEVEL 1
#else
#define BTN_PRESSED_LEVEL  1
#define BTN_RELEASED_LEVEL 0
#endif

#define LOG_LOCAL_LEVEL CONFIG_BTN_LOG_LEVEL
#include "esp_log.h"
#ifndef LOG_TAG
#define LOG_TAG "BTN_EVENTS"
#endif

static gptimer_handle_t gptimer = NULL;
static int buttons_count = 0;

static void start_timer()
{
	if (gptimer != NULL)
	{
		return;
	}
	gptimer_config_t timer_config = {
		.clk_src = GPTIMER_CLK_SRC_DEFAULT,
		.direction = GPTIMER_COUNT_UP,
		.resolution_hz = 1 * 1000 * 1000, // 1MHz, 1 tick = 1us
	};
	ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));
	ESP_ERROR_CHECK(gptimer_enable(gptimer));
	ESP_ERROR_CHECK(gptimer_start(gptimer));
}

static void stop_timer()
{
	ESP_ERROR_CHECK(gptimer_stop(gptimer));
	ESP_ERROR_CHECK(gptimer_disable(gptimer));
	gptimer = NULL;
}

static void button_pressed_event(button_events_t *button)
{
	ESP_LOGD(LOG_TAG, "Press event");
	if (button->btn_events_pressed_cb != NULL)
	{
		button->btn_events_pressed_cb(button->button);
	}
}

static void button_hold_event(button_events_t *button)
{
	ESP_LOGD(LOG_TAG, "Hold event");
	if (button->btn_events_hold_cb != NULL)
	{
		button->btn_events_hold_cb(button->button);
	}
}

static void button_released_event(button_events_t *button)
{
	ESP_LOGD(LOG_TAG, "Release event");
	if (button->btn_events_released_cb != NULL)
	{
		button->btn_events_released_cb(button->button);
	}
}

static void button_click_event(button_events_t *button)
{
	ESP_LOGD(LOG_TAG, "Click event");
	if (button->btn_events_click_cb != NULL)
	{
		button->btn_events_click_cb(button->button);
	}
}

static void check_button(void *param)
{
	button_events_t *button = (button_events_t *)param;
	ESP_LOGD(LOG_TAG, "Check button (button: %d, hold time: %lld, pin: %d).", button->button,
		 button->hold_time, button->pin);
	int last_state = 1;

	int current_state = gpio_get_level(button->pin);
	uint64_t uptime = 0;
	uint64_t last_time = 0;
	ESP_ERROR_CHECK(gptimer_get_raw_count(gptimer, &last_time));

	bool parsed = false;

	while (true)
	{
		current_state = gpio_get_level(button->pin);
		ESP_ERROR_CHECK(gptimer_get_raw_count(gptimer, &uptime));

		if (current_state == last_state &&
		    (uptime - last_time < button->hold_time || parsed))
		{
			;
		}
		if (current_state == last_state && current_state == BTN_PRESSED_LEVEL &&
		    (uptime - last_time >= button->hold_time && !parsed))
		{
			button_hold_event(button);
			parsed = true;
		}
		if (current_state != last_state)
		{
			switch (current_state)
			{
			case BTN_PRESSED_LEVEL:
				button_pressed_event(button);
				parsed = false;
				break;

			case BTN_RELEASED_LEVEL:
				if (button->btn_events_released_cb != NULL)
				{
					button_released_event(button);
				}
				if (!parsed)
				{
					button_click_event(button);
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

int btn_events_init(button_events_t *button)
{
	esp_log_level_set(LOG_TAG, LOG_LOCAL_LEVEL);
	ESP_LOGD(LOG_TAG, "Initiating button events dispatcher...");
	if (button->task_handle != NULL)
	{
		ESP_LOGE(LOG_TAG, "Button is initializated already.");
		return -1;
	}

	if (button->hold_time == 0)
	{
		button->hold_time = DEFAULT_BTN_HOLD_TIME_MS;
	}

	start_timer();

	++buttons_count;
	return 0;
}

void btn_event_start_task(button_events_t *button)
{
	ESP_LOGD(LOG_TAG, "Starting task for button %d...", button->button);
	assert(button->button <= 999);

	// char num[3];
	// itoa(button->button, num, 10);
	// char taskname[6];
	// mempcpy(taskname, "BTN", 3);
	// mempcpy(taskname + 3, num, 3);
	xTaskCreate(check_button, "BTN_EVENTS", 3000, (void *)button, tskIDLE_PRIORITY,
		    &(button->task_handle));
	configASSERT(button->task_handle);
	ESP_LOGD(LOG_TAG, "Task started.");
}

void btn_event_stop_task(button_events_t *button)
{
	ESP_LOGD(LOG_TAG, "Stoping task for button %d...", button->button);
	if (buttons_count <= 0)
	{
		ESP_LOGE(LOG_TAG, "No more inited buttons found.");
		return;
	}
	if (button->task_handle != NULL)
	{
		vTaskDelete(button->task_handle);
	}
	--buttons_count;
	if (buttons_count == 0)
	{
		stop_timer();
	}
	ESP_LOGD(LOG_TAG, "Task stoped.");
}

bool is_btn_pressed(button_events_t *button)
{
	return gpio_get_level(button->pin) == BTN_PRESSED_LEVEL;
}
