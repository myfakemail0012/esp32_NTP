#include <stdio.h>

#include "sdkconfig.h"

#include "app_spi.h"

#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/spi_master.h"

#define LOG_LOCAL_LEVEL CONFIG_SPI_LOG_LEVEL
#include "esp_log.h"
#ifndef LOG_TAG
#define LOG_TAG "ASPI"
#endif

#define ASPI_HOST HSPI_HOST

#ifndef CONFIG_APP_SPI_CLOCK_MHZ
#define CONFIG_APP_SPI_CLOCK_MHZ 8
#endif // CONFIG_APP_SPI_CLOCK_MHZ

#define ASPI_CLK_MHZ        CONFIG_APP_SPI_CLOCK_MHZ
#define ASPI_CLK_FREQ       (ASPI_CLK_MHZ * 1000 * 1000)
#define ASPI_INPUT_DELAY_NS ((1000 * 1000 * 1000 / ASPI_CLK_FREQ) / 2 + 20)

static spi_device_handle_t spi;

esp_err_t aspi_init(aspi_config_t config)
{
	esp_log_level_set(LOG_TAG, LOG_LOCAL_LEVEL);

	esp_err_t ret;

	spi_bus_config_t buscfg = {
		.miso_io_num = config.miso_pin,
		.mosi_io_num = config.mosi_pin,
		.sclk_io_num = config.sck_pin,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = 32,
	};
	spi_device_interface_config_t devcfg = {
		.clock_speed_hz = ASPI_CLK_FREQ, // 10 MHz
		.mode = 0,                       // SPI mode 0
		.spics_io_num = config.cs_pin,   // CS pin

		.command_bits = 8,
		.queue_size = 1,
		// .flags = ,
		// .pre_cb = cs_high,
		// .post_cb = cs_low,
		.input_delay_ns = ASPI_INPUT_DELAY_NS,

	};

	ret = spi_bus_initialize(ASPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "Failed to initialize spi bus: %s (0x%x)", esp_err_to_name(ret),
			 ret);
		return ret;
	}

	ret = spi_bus_add_device(ASPI_HOST, &devcfg, &spi);
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "Failed to add bus device: %s (0x%x)", esp_err_to_name(ret), ret);
		spi_bus_free(ASPI_HOST);
		return ret;
	}
	return ESP_OK;
}

esp_err_t aspi_deinit(void)
{
	esp_err_t ret;
	ret = spi_bus_remove_device(spi);
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "Failed to remove device: %s (0x%x)", esp_err_to_name(ret), ret);
		return ret;
	}

	ret = spi_bus_free(ASPI_HOST);
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "Failed to bus free: %s (0x%x)", esp_err_to_name(ret), ret);
		return ret;
	}

	return ESP_OK;
}

esp_err_t aspi_transmit_recieve(aspi_transaction_t *transaction)
{
	esp_err_t ret;
	if (transaction == NULL)
	{
		ESP_LOGE(LOG_TAG, "transaction can't be NULL");
		return ESP_ERR_INVALID_ARG;
	}

	spi_transaction_t data = {
		.cmd = transaction->cmd,
		.rx_buffer = transaction->in_data,
		.rxlength = transaction->in_data_len,
		.tx_buffer = transaction->out_data,
		.length = (transaction->out_data_len + transaction->in_data_len) * 8,
	};

	ret = spi_device_transmit(spi, &data);
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "Failed to spi transmit: %s (0x%x)", esp_err_to_name(ret), ret);
		return ret;
	}
	return ESP_OK;
}
