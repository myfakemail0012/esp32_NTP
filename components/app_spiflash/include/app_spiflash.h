#pragma once

#include "esp_flash_spi_init.h"

typedef enum
{
	ASPI_IDLE,
	ASPI_INITED,
} aspi_status_e;

esp_err_t aspi_init(void);
esp_err_t aspi_deinit(void);
esp_err_t aspi_read(uint32_t addr, uint8_t *read_buf, size_t length);
esp_err_t aspi_write(uint32_t addr, uint8_t *write_buf, size_t length);
esp_err_t aspi_erase_chip();
esp_err_t aspi_erase_sector(uint32_t addr);
aspi_status_e aspi_get_status();
esp_err_t aspi_wait_until_free();
bool aspi_is_busy();