#pragma once
#include <string.h>

#include "esp_nimble_cfg.h"

typedef enum
{
	ABLE_IDLE = 0,
	ABLE_INITED,
	ABLE_DISCONNECTED,
	ABLE_CONNECTED,
	ABLE_ADV,
} able_state_t;

int able_init();
// void able_deinit(); //????
int able_start_advertising();
void able_stop_advertising(); //?????
void able_write_all(char *data, size_t data_len);
void able_notify_all(char *data, size_t data_len);
able_state_t able_get_state();
// void able_set_power();
// void able_get_power();
void able_set_bat_level(int level);
