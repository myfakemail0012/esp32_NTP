#include <stdio.h>
#include <string.h>
#include "app_spi.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "esp_flash_spi_init.h"

#define APP_SPI_CS_PIN   25
#define APP_SPI_CLK_PIN  26
#define APP_SPI_MOSI_PIN 33
#define APP_SPI_MISO_PIN 35
// #define APP_SPI_IO0_PIN 33
// #define APP_SPI_IO1_PIN 35
// #define APP_SPI_IO2_PIN 32
// #define APP_SPI_IO3_PIN 33
#define EEPROM_HOST      HSPI_HOST

#define SPI_DMA_CHAN SPI_DMA_CH_AUTO

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#ifndef LOG_TAG
#define LOG_TAG "ASPI"
#endif

static esp_flash_t *espflash;

esp_err_t aspi_init(void)
{
	esp_err_t ret;
	ESP_LOGD(LOG_TAG, "Initializing bus SPI%d...", EEPROM_HOST + 1);
	spi_bus_config_t buscfg = {
		.miso_io_num = APP_SPI_MISO_PIN,
		.mosi_io_num = APP_SPI_MOSI_PIN,
		.sclk_io_num = APP_SPI_CLK_PIN,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = 32,
	};

	// The input only Pins are handled here!
	ret = gpio_set_direction(buscfg.miso_io_num, GPIO_MODE_INPUT);
	if (ret != 0)
	{
		ESP_LOGE(LOG_TAG, "Failed to set gpio direction: %s (0x%x)", esp_err_to_name(ret),
			 ret);
		return ret;
	}
	ret = gpio_set_direction(buscfg.mosi_io_num, GPIO_MODE_OUTPUT);
	if (ret != 0)
	{
		ESP_LOGE(LOG_TAG, "Failed to set gpio direction: %s (0x%x)", esp_err_to_name(ret),
			 ret);
		return ret;
	}

	esp_flash_spi_device_config_t devconfig = {
		.host_id = EEPROM_HOST,
		.cs_id = 0,
		.cs_io_num = APP_SPI_CS_PIN,
		.io_mode = SPI_FLASH_SLOWRD,
		.freq_mhz = 40,
	};

	// Initialize the SPI bus
	ESP_LOGD(LOG_TAG, "Initializing external SPI Flash");
	ESP_LOGD(LOG_TAG, "Pin assignments:");
	ESP_LOGD(LOG_TAG, "MOSI: %2d   MISO: %2d   SCLK: %2d   CS: %2d", buscfg.mosi_io_num,
		 buscfg.miso_io_num, buscfg.sclk_io_num, devconfig.cs_io_num);
	ESP_LOGD(LOG_TAG, "DMA CHANNEL: %d", SPI_DMA_CHAN);

	ret = spi_bus_initialize(EEPROM_HOST, &buscfg, SPI_DMA_CHAN);
	if (ret != 0)
	{
		ESP_LOGE(LOG_TAG, "Failed to initialize spi bus: %s (0x%x)", esp_err_to_name(ret),
			 ret);
		return ret;
	}

	ret = spi_bus_add_flash_device(&espflash, &devconfig);
	if (ret != 0)
	{
		ESP_LOGE(LOG_TAG, "Failed to add flash device: %s (0x%x)", esp_err_to_name(ret),
			 ret);
		return ret;
	}

	// Probe the Flash chip and initialize it
	ret = esp_flash_init(espflash);
	if (ret != ESP_OK)
	{
		ESP_LOGE(LOG_TAG, "Failed to initialize external Flash: %s (0x%x)",
			 esp_err_to_name(ret), ret);
		return ret;
	}

	uint32_t id;
	ret = esp_flash_read_id(espflash, &id);
	if (ret != 0)
	{
		return ret;
	}
	ESP_LOGI(LOG_TAG, "Initialized external Flash, size=%" PRIu32 " KB, ID=0x%" PRIx32,
		 espflash->size / 1024, id);

	return ESP_OK;
}

esp_err_t aspi_deinit(void)
{
	return spi_bus_remove_flash_device(espflash);
}

esp_err_t aspi_read(uint64_t addr, uint8_t *read_buf, size_t length)
{
	ESP_LOGD(LOG_TAG, "Reading %d bytes from %lld", length, addr);
	esp_err_t ret = esp_flash_read(espflash, read_buf, addr, length);
	if (ret != ESP_OK)
	{
		ESP_LOGE(LOG_TAG, "Failed to read Flash: %s (0x%x)", esp_err_to_name(ret), ret);
		return ret;
	}
	return ESP_OK;
}

esp_err_t aspi_write(uint64_t addr, uint8_t *write_buf, size_t length)
{
	ESP_LOGD(LOG_TAG, "Writing %d bytes from %lld", length, addr);
	esp_err_t ret = esp_flash_write(espflash, write_buf, addr, length);
	if (ret != ESP_OK)
	{
		ESP_LOGE(LOG_TAG, "Failed to write Flash: %s (0x%x)", esp_err_to_name(ret), ret);
		return ret;
	}
	return ESP_OK;
}

esp_err_t aspi_erase_chip()
{
	return esp_flash_erase_chip(espflash);
}