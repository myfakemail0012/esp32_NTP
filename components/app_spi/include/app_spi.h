#pragma once

#include "esp_err.h"

/***
 * @brief SPI Bus settings. Pins definition.
 */
typedef struct
{
	uint8_t mosi_pin;
	uint8_t miso_pin;
	uint8_t sck_pin;
	uint8_t cs_pin;
} aspi_config_t;

/***
 * @brief ASPI transaction structure. Describes command and data for transmitting and receiving to
 * and from SPI
 */
typedef struct
{
	uint16_t cmd;        /* Command code. Can be 8-16bit */
	uint8_t *in_data;    /* Pointer to receive data buffer. */
	size_t in_data_len;  /* Recieve data length. */
	uint8_t *out_data;   /* Pointer to transmit data buffer. */
	size_t out_data_len; /* Recieve data length. */
} aspi_transaction_t;

/***
 * @brief Initialize SPI Bus.
 * @param config aspi_config_t structure with pins definition
 * @return ESP_OK if success, ESP_ERR_* otherwise
 */
esp_err_t aspi_init(aspi_config_t config);

/***
 * @brief Deinitialize SPI Bus.
 * @return ESP_OK if success, ESP_ERR_* otherwise
 */
esp_err_t aspi_deinit(void);

/***
 * @brief Transmit and receive data to and from SPI using specified command.
 * @param transaction Pointer to ASPI transaction structure with data configuration
 * @return ESP_OK if success, ESP_ERR_* otherwise
 */
esp_err_t aspi_transmit_recieve(aspi_transaction_t *transaction);
