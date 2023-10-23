#pragma once

#include "esp_flash_spi_init.h"

typedef enum
{
	ASPI_IDLE,
	ASPI_INITED,
} aspi_flash_status_e;

esp_err_t aspi_flash_init(void);
esp_err_t aspi_flash_deinit(void);
esp_err_t aspi_flash_read(uint32_t addr, uint8_t *read_buf, size_t length);
esp_err_t aspi_flash_write(uint32_t addr, uint8_t *write_buf, size_t length);
esp_err_t aspi_flash_erase_chip();
esp_err_t aspi_flash_erase_sector(uint32_t addr);
aspi_flash_status_e aspi_flash_get_status();
esp_err_t aspi_flash_wait_until_free();
bool aspi_flash_is_busy();