#include <string.h>
#include "unity.h"
#include "app_spiflash.h"
#include "esp_err.h"
#include "inttypes.h"

#define CHIP_SIZE (16384 * 1024)

TEST_CASE("SPI test init / deinit", "[init/deinit/pass]")
{
	TEST_ASSERT_EQUAL(ASPI_IDLE, aspi_get_status());

	esp_err_t ret = aspi_init();
	TEST_ASSERT_EQUAL(ESP_OK, ret);

	ret = aspi_deinit();
	TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("SPI test read null buffer", "[read/fail]")
{
	esp_err_t ret = aspi_read(0, NULL, 0);
	TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}
TEST_CASE("SPI test read out of range", "[read/fail]")
{
	uint8_t arr[1];
	esp_err_t ret = aspi_read(0, arr, 100000000);
	TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, ret);
}

TEST_CASE("SPI test write null buffer", "[write/fail]")
{
	esp_err_t ret = aspi_write(0, NULL, 0);
	TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}
TEST_CASE("SPI test write out of range", "[write/fail]")
{
	uint8_t arr[1];
	esp_err_t ret = aspi_write(0, arr, 100000000);
	TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, ret);
}

TEST_CASE("SPI test write / read / erase", "[read/write/erase/pass]")
{
	char *txt = "My test string";

	const size_t txt_len = strlen(txt);
	const unsigned long long addr = 0x12AB;

	esp_err_t ret;

	if (aspi_get_status() == ASPI_IDLE)
	{
		ret = aspi_init();
		TEST_ASSERT_EQUAL(ESP_OK, ret);
	}

	ret = aspi_erase_sector(addr);
	TEST_ASSERT_EQUAL(ESP_OK, ret);

	aspi_wait_until_free();

	ret = aspi_write(addr, (uint8_t *)txt, txt_len);
	TEST_ASSERT_EQUAL(ESP_OK, ret);

	aspi_wait_until_free();

	char *read_buf[txt_len];
	ret = aspi_read(addr, (uint8_t *)read_buf, txt_len);
	TEST_ASSERT_EQUAL(ESP_OK, ret);
	TEST_ASSERT_EQUAL_STRING_LEN(txt, read_buf, txt_len);

	ret = aspi_erase_sector(addr);
	TEST_ASSERT_EQUAL(ESP_OK, ret);

	aspi_wait_until_free();

	uint8_t data;

	ret = aspi_read(addr, &data, 1);
	TEST_ASSERT_EQUAL(ESP_OK, ret);
	TEST_ASSERT_EQUAL(0xFF, data);

	ret = aspi_deinit();
	TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("SPI test erase chip", "[erase/pass]")
{
	uint8_t data = 0xEF;

	const unsigned long long addr = 0x20;

	esp_err_t ret;

	if (aspi_get_status() == ASPI_IDLE)
	{
		ret = aspi_init();
		TEST_ASSERT_EQUAL(ESP_OK, ret);
	}
	ret = aspi_erase_sector(addr);
	TEST_ASSERT_EQUAL(ESP_OK, ret);

	aspi_wait_until_free();

	ret = aspi_write(addr, &data, 1);
	TEST_ASSERT_EQUAL(ESP_OK, ret);

	aspi_wait_until_free();

	uint8_t read_data;
	ret = aspi_read(addr, &read_data, 1);
	TEST_ASSERT_EQUAL(ESP_OK, ret);
	TEST_ASSERT_EQUAL(data, read_data);

	ret = aspi_erase_chip();
	TEST_ASSERT_EQUAL(ESP_OK, ret);

	aspi_wait_until_free();

	ret = aspi_read(addr, &read_data, 1);
	esp_err_t ret1 = aspi_deinit();
	TEST_ASSERT_EQUAL(ESP_OK, ret);
	TEST_ASSERT_EQUAL(ESP_OK, ret1);
	TEST_ASSERT_EQUAL(0xFF, read_data);
}