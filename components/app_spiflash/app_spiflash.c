#include <stdio.h>
#include <string.h>
#include "app_spiflash.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/task.h"

#include "sdkconfig.h"

#include "spi_flash_chip_driver.h"
#include "esp_flash_spi_init.h"

#define APP_SPI_CHIPERASE_TIMEOUT CONFIG_SPI_CHIPERASE_TIMEOUT_MS

#define APP_SPI_CS_PIN   CONFIG_SPI_CS_PIN
#define APP_SPI_CLK_PIN  CONFIG_SPI_SCK_PIN
#define APP_SPI_MOSI_PIN CONFIG_SPI_MOSI_PIN
#define APP_SPI_MISO_PIN CONFIG_SPI_MISO_PIN
// #define APP_SPI_IO0_PIN 33
// #define APP_SPI_IO1_PIN 35
// #define APP_SPI_IO2_PIN 32
// #define APP_SPI_IO3_PIN 33
#define SPIFLASH_HOST    HSPI_HOST

#define SPI_DMA_CHAN SPI_DMA_CH_AUTO

#define LOG_LOCAL_LEVEL CONFIG_SPIFLASH_LOG_LEVEL
#include "esp_log.h"
#ifndef LOG_TAG
#define LOG_TAG "ASPIFLASH"
#endif

static esp_flash_t *espflash;

static aspi_flash_status_e aspi_flash_flash_status = ASPI_IDLE;

static esp_err_t check_write_protect_and_disable()
{
	bool write_protect;
	esp_err_t ret = esp_flash_get_chip_write_protect(espflash, &write_protect);
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "Failed to get write protect status: %s (0x%x)",
			 esp_err_to_name(ret), ret);
		return ret;
	}
	if (write_protect)
	{
		ESP_LOGI(LOG_TAG, "Write protect is enabled. Try to disable write protect.");
		ret = esp_flash_set_chip_write_protect(espflash, false);
		if (ret)
		{
			ESP_LOGE(LOG_TAG, "Failed to disable chip write protect: %s (0x%x)",
				 esp_err_to_name(ret), ret);
			return ret;
		}
	}
	return ESP_OK;
}

static esp_err_t check_flash_busy(esp_flash_t *chip, bool *busy)
{
	uint8_t status_reg1;

	esp_err_t ret;
	spi_flash_trans_t trans = {
		.command = 0x05,
		.miso_data = &status_reg1,
		.miso_len = 1,
	};

	ret = chip->host->driver->common_command(chip->host, &trans);
	if (ret != ESP_OK)
	{
		ESP_LOGE(LOG_TAG, "Failed to read status register: %s (0x%x)", esp_err_to_name(ret),
			 ret);
		return ret;
	}
	*busy = status_reg1 & 1;
	return ESP_OK;
}

static int32_t wait_until_free(esp_flash_t *chip)
{
	ESP_LOGD(LOG_TAG, "Waiting until chip will be free.");
	TickType_t start = xTaskGetTickCount();
	bool is_busy = true;
	while (is_busy)
	{
		check_flash_busy(chip, &is_busy);
		TickType_t end = xTaskGetTickCount();
		if ((end - start) * 1000 / CONFIG_FREERTOS_HZ > APP_SPI_CHIPERASE_TIMEOUT)
		{
			ESP_LOGE(LOG_TAG, "Timeout!!!");
			return -ESP_ERR_TIMEOUT;
		}
		vTaskDelay(1);
	}

	return (xTaskGetTickCount() - start) * 1000 / CONFIG_FREERTOS_HZ;
}

static esp_err_t impl_erase_chip(esp_flash_t *chip)
{
	esp_err_t ret;

	ESP_LOGD(LOG_TAG, "Erasing chip");

	ret = check_write_protect_and_disable();
	if (ret != ESP_OK)
	{
		ESP_LOGE(LOG_TAG, "Failed to disable write protect: %s (0x%x)",
			 esp_err_to_name(ret), ret);
		return ret;
	}

	spi_flash_trans_t trans = {
		.command = 0x60,
	};

	ret = chip->host->driver->common_command(chip->host, &trans);
	if (ret != ESP_OK)
	{
		ESP_LOGE(LOG_TAG, "Failed to send erase command: %s (0x%x)", esp_err_to_name(ret),
			 ret);
		return ret;
	}

	// int32_t erase_time = wait_until_free(espflash);
	// if (erase_time < 0)
	// {
	// 	ESP_LOGE(LOG_TAG, "Failed to erase. Timeout");
	// 	return ESP_ERR_TIMEOUT;
	// }
	// ESP_LOGD(LOG_TAG, "Chip erasing got %ldms", erase_time);
	return ESP_OK;
}

esp_err_t aspi_flash_init(void)
{
	esp_log_level_set(LOG_TAG, LOG_LOCAL_LEVEL);
	esp_err_t ret;
	ESP_LOGD(LOG_TAG, "Initializing bus SPI%d...", SPIFLASH_HOST + 1);
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
		.host_id = SPIFLASH_HOST,
		.cs_id = 0,
		.cs_io_num = APP_SPI_CS_PIN,
		.io_mode = SPI_FLASH_SLOWRD,
		.freq_mhz = ESP_FLASH_40MHZ,
	};

	// Initialize the SPI bus
	ESP_LOGD(LOG_TAG, "Initializing external SPI Flash");
	ESP_LOGD(LOG_TAG, "Pin assignments:");
	ESP_LOGD(LOG_TAG, "MOSI: %2d   MISO: %2d   SCLK: %2d   CS: %2d", buscfg.mosi_io_num,
		 buscfg.miso_io_num, buscfg.sclk_io_num, devconfig.cs_io_num);
	ESP_LOGD(LOG_TAG, "DMA CHANNEL: %d", SPI_DMA_CHAN);

	ret = spi_bus_initialize(SPIFLASH_HOST, &buscfg, SPI_DMA_CHAN);
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

	// espflash->host->driver->erase_chip = impl_erase_chip;
	aspi_flash_flash_status = ASPI_INITED;
	return ESP_OK;
}

esp_err_t aspi_flash_deinit(void)
{
	esp_err_t ret;

	ret = spi_bus_remove_flash_device(espflash);
	if (ret != ESP_OK)
	{
		ESP_LOGE(LOG_TAG, "Failed to remove flash device: %s (0x%x)", esp_err_to_name(ret),
			 ret);
		return ret;
	}
	ret = spi_bus_free(SPIFLASH_HOST);
	if (ret != ESP_OK)
	{
		ESP_LOGE(LOG_TAG, "Failed to free spi bus: %s (0x%x)", esp_err_to_name(ret), ret);
		return ret;
	}
	aspi_flash_flash_status = ASPI_IDLE;
	return ESP_OK;
}

esp_err_t aspi_flash_read(uint32_t addr, uint8_t *read_buf, size_t length)
{
	ESP_LOGD(LOG_TAG, "Reading...");
	if (espflash->busy)
	{
		ESP_LOGE(LOG_TAG, "Error. SPI flash is busy.");
		return ESP_ERR_TIMEOUT;
	}
	if (read_buf == NULL)
	{
		ESP_LOGE(LOG_TAG, "read_buf can't be NULL.");
		return ESP_ERR_INVALID_ARG;
	}

	if (length + addr > espflash->size)
	{
		ESP_LOGE(LOG_TAG, "Out of range.");
		return ESP_ERR_INVALID_SIZE;
	}

	ESP_LOGD(LOG_TAG, "Reading %d bytes from address [0x%0X%0X]", length,
		 (uint16_t)(addr >> 16), (uint16_t)(addr - ((addr >> 16) << 16)));
	esp_err_t ret = esp_flash_read(espflash, read_buf, addr, length);
	if (ret != ESP_OK)
	{
		ESP_LOGE(LOG_TAG, "Failed to read Flash: %s (0x%x)", esp_err_to_name(ret), ret);
		return ret;
	}
	return ESP_OK;
}

esp_err_t aspi_flash_write(uint32_t addr, uint8_t *write_buf, size_t length)
{
	ESP_LOGD(LOG_TAG, "Writing...");

	if (write_buf == NULL)
	{
		ESP_LOGE(LOG_TAG, "write_buf can't be NULL.");
		return ESP_ERR_INVALID_ARG;
	}

	if (length + addr > espflash->size)
	{
		ESP_LOGE(LOG_TAG, "Out of range.");
		return ESP_ERR_INVALID_SIZE;
	}

	bool busy;
	check_flash_busy(espflash, &busy);
	if (busy)
	{
		ESP_LOGE(LOG_TAG, "Error. SPI flash is busy.");
		return ESP_ERR_TIMEOUT;
	}

	ESP_LOGD(LOG_TAG, "Writing %d bytes to address [0x%0X%0X]", length, (uint16_t)(addr >> 16),
		 (uint16_t)(addr - ((addr >> 16) << 16)));
	esp_err_t ret = esp_flash_write(espflash, write_buf, addr, length);
	if (ret != ESP_OK)
	{
		ESP_LOGE(LOG_TAG, "Failed to write Flash: %s (0x%x)", esp_err_to_name(ret), ret);
		return ret;
	}
	return ESP_OK;
}

esp_err_t aspi_flash_erase_chip()
{
	bool busy;
	check_flash_busy(espflash, &busy);
	if (busy)
	{
		ESP_LOGE(LOG_TAG, "Error. SPI flash is busy.");
		return ESP_ERR_TIMEOUT;
	}
	return impl_erase_chip(espflash);
	// return esp_flash_erase_chip(espflash); // Standart erasing is erase all chip by region
	// erase
}

esp_err_t aspi_flash_erase_sector(uint32_t addr)
{
	ESP_LOGD(LOG_TAG, "Erasing sector");

	bool busy;
	check_flash_busy(espflash, &busy);
	if (busy)
	{
		ESP_LOGE(LOG_TAG, "[%d] Error. SPI flash is busy.", __LINE__);
		return ESP_ERR_TIMEOUT;
	}

	esp_err_t ret;
	ret = check_write_protect_and_disable();
	if (ret != ESP_OK)
	{
		ESP_LOGE(LOG_TAG, "Failed to disable write protect: %s (0x%x)",
			 esp_err_to_name(ret), ret);
		return ret;
	}

	const spi_flash_chip_t *chip_drv = espflash->chip_drv;
	const uint32_t sectorSize = chip_drv->sector_size;
	const uint32_t sector = addr / sectorSize;
	ret = esp_flash_erase_region(espflash, sector * sectorSize, sectorSize);
	if (ret != ESP_OK)
	{
		ESP_LOGE(LOG_TAG, "Failed to erase region: %s (0x%x)", esp_err_to_name(ret), ret);
		ESP_LOGE(LOG_TAG, "addr: %ld, sectorSize: %ld, calculated sector: %ld", addr,
			 sectorSize, sector);
		return ret;
	}
	int32_t erase_time = wait_until_free(espflash);
	if (erase_time < 0)
	{
		return ESP_ERR_TIMEOUT;
	}
	ESP_LOGD(LOG_TAG, "Sector erasing got %ldms", erase_time);
	return ESP_OK;
}

aspi_flash_status_e aspi_flash_get_status()
{
	return aspi_flash_flash_status;
}

esp_err_t aspi_flash_wait_until_free()
{
	int32_t wait_time = wait_until_free(espflash);
	if (wait_time < 0)
	{
		ESP_LOGE(LOG_TAG, "Timeout!!!");
		return ESP_ERR_TIMEOUT;
	}
	ESP_LOGD(LOG_TAG, "Waited %ldms", wait_time);
	return ESP_OK;
}

bool aspi_flash_is_busy()
{
	if (!esp_flash_chip_driver_initialized(espflash))
	{
		ESP_LOGE(LOG_TAG, "Flash driver not initialized");
		aspi_flash_flash_status = ASPI_IDLE;
		return false;
	}
	bool busy;
	check_flash_busy(espflash, &busy);
	return busy;
}
