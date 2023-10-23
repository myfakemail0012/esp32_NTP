#include <string.h>
#include "unity.h"
#include "app_spi.h"
#include "esp_err.h"
#include "inttypes.h"

#define CHIP_SIZE (16384 * 1024)

TEST_CASE("SPI test init / deinit", "[ASPI]")
{
	aspi_config_t config = {
		.miso_pin = 35,
		.mosi_pin = 33,
		.sck_pin = 26,
		.cs_pin = 25,
	};
	esp_err_t ret = aspi_init(config);
	TEST_ASSERT_EQUAL(ESP_OK, ret);

	ret = aspi_deinit();
	TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("SPI test send command / get answer", "[ASPI]")
{
	aspi_config_t config = {
		.miso_pin = 35,
		.mosi_pin = 33,
		.sck_pin = 26,
		.cs_pin = 25,
	};
	esp_err_t ret = aspi_init(config);
	TEST_ASSERT_EQUAL(ESP_OK, ret);

	uint8_t buf[3];
	uint8_t ibuf[3];

	aspi_transaction_t trans = {
		.cmd = 0x9F,
		.in_data = buf,
		.in_data_len = 3,
		.out_data_len = 0,
		.out_data = ibuf,
	};
	ret = aspi_transmit_recieve(&trans);
	TEST_ASSERT_EQUAL(ESP_OK, ret);
	TEST_ASSERT_EQUAL(0xEf, buf[0]);
	TEST_ASSERT_EQUAL(0x40, buf[1]);
	TEST_ASSERT_EQUAL(0x18, buf[2]);

	ret = aspi_deinit();
	TEST_ASSERT_EQUAL(ESP_OK, ret);
}