#pragma once

#include "esp_flash_spi_init.h"

esp_err_t aspi_init(void);
esp_err_t aspi_deinit(void);
esp_err_t aspi_read(uint64_t addr, uint8_t *read_buf, size_t length);
esp_err_t aspi_write(uint64_t addr, uint8_t *write_buf, size_t length);
