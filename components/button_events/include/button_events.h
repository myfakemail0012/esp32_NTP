#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "driver/gpio.h"

#define DEFAULT_BTN_HOLD_TIME_MS CONFIG_DEFAULT_BTN_HOLD_TIME_MS

/**
 * @typedef btn_event_cb_t
 * @brief Button event call back function
 * @param button Button nummer settet in @ref button_events_t->button
 */
typedef void (*btn_event_cb_t)(int button);

/**
 * @typedef button_events_t
 * @brief Button configuration
 */
typedef struct s_button_events
{
	int pin;
	unsigned int button;
	uint64_t hold_time;
	TaskHandle_t task_handle;
	btn_event_cb_t btn_events_pressed_cb;
	btn_event_cb_t btn_events_released_cb;
	btn_event_cb_t btn_events_hold_cb;
	btn_event_cb_t btn_events_click_cb;
} button_events_t;

/**
 * @brief Initialize event dispatcher for button.
 * @param button pointer to button_events_t structure.
 * @result write task handler to button->task_handler.
 */
int btn_events_init(button_events_t *button);

/**
 * @brief Create new task and start parsing button state.
 * 			Call callback function on press, release, hold and click events.
 * @param button pointer to button_events_t structure
 */
void btn_event_start_task(button_events_t *button);

/**
 * @brief Delete task and stop parsing for button.
 * @param button pointer to button_events_t structure.
 */
void btn_event_stop_task(button_events_t *button);