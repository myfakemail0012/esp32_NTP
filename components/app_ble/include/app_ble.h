#pragma once
#include <string.h>
#include "esp_err.h"

typedef enum
{
	ABLE_IDLE = 0,
	ABLE_INITED,
	ABLE_DISCONNECTED,
	ABLE_CONNECTED,
	ABLE_ADV,
} able_state_t;

// typedef enum
// {
// 	ABLE_PWR_LVL_N12 = 0, /*!< Corresponding to -12dbm */
// 	ABLE_PWR_LVL_N9 = 1,  /*!< Corresponding to  -9dbm */
// 	ABLE_PWR_LVL_N6 = 2,  /*!< Corresponding to  -6dbm */
// 	ABLE_PWR_LVL_N3 = 3,  /*!< Corresponding to  -3dbm */
// 	ABLE_PWR_LVL_N0 = 4,  /*!< Corresponding to   0dbm */
// 	ABLE_PWR_LVL_P3 = 5,  /*!< Corresponding to  +3dbm */
// 	ABLE_PWR_LVL_P6 = 6,  /*!< Corresponding to  +6dbm */
// 	ABLE_PWR_LVL_P9 = 7,  /*!< Corresponding to  +9dbm */
// } able_powers_e;

// int able_get_power_level_from_enum(able_powers_e lev);

esp_err_t able_init();
// void able_deinit(); //????
int able_start_advertising();
void able_stop_advertising();
void able_write_all(char *data, size_t data_len);
void able_notify_all(char *data, size_t data_len);
able_state_t able_get_state();
// int able_set_tx_power_level(able_powers_e min, able_powers_e max);
// int able_get_tx_power_level(able_powers_e *min, able_powers_e *max);
void able_set_bat_level(uint8_t level);
void able_clear_bonded(void);
uint8_t able_get_active_connection_count();
