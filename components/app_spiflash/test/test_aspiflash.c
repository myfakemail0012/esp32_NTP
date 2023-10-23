#include <string.h>
#include "unity.h"
#include "app_spiflash.h"
#include "esp_err.h"
#include "inttypes.h"

#define CHIP_SIZE (16384 * 1024)

TEST_CASE("SPI_FLASH test init / deinit", "[ASPIFLASH]")
{
	TEST_ASSERT_EQUAL(ASPI_IDLE, aspi_flash_get_status());

	esp_err_t ret = aspi_flash_init();
	TEST_ASSERT_EQUAL(ESP_OK, ret);

	ret = aspi_flash_deinit();
	TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("SPI_FLASH test read null buffer", "[ASPIFLASH]")
{
	esp_err_t ret = aspi_flash_read(0, NULL, 0);
	TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}
TEST_CASE("SPI_FLASH test read out of range", "[ASPIFLASH]")
{
	uint8_t arr[1];
	esp_err_t ret = aspi_flash_read(0, arr, 100000000);
	TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, ret);
}

TEST_CASE("SPI_FLASH test write null buffer", "[ASPIFLASH]")
{
	esp_err_t ret = aspi_flash_write(0, NULL, 0);
	TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}
TEST_CASE("SPI_FLASH test write out of range", "[ASPIFLASH]")
{
	uint8_t arr[1];
	esp_err_t ret = aspi_flash_write(0, arr, 100000000);
	TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, ret);
}

TEST_CASE("SPI_FLASH test write / read / erase", "[ASPIFLASH]")
{
	char *txt = "My test string";

	const size_t txt_len = strlen(txt);
	const unsigned long long addr = 0x12AB;

	esp_err_t ret;

	if (aspi_flash_get_status() == ASPI_IDLE)
	{
		ret = aspi_flash_init();
		TEST_ASSERT_EQUAL(ESP_OK, ret);
	}

	ret = aspi_flash_erase_sector(addr);
	TEST_ASSERT_EQUAL(ESP_OK, ret);

	aspi_flash_wait_until_free();

	ret = aspi_flash_write(addr, (uint8_t *)txt, txt_len);
	TEST_ASSERT_EQUAL(ESP_OK, ret);

	aspi_flash_wait_until_free();

	char *read_buf[txt_len];
	ret = aspi_flash_read(addr, (uint8_t *)read_buf, txt_len);
	TEST_ASSERT_EQUAL(ESP_OK, ret);
	TEST_ASSERT_EQUAL_STRING_LEN(txt, read_buf, txt_len);

	ret = aspi_flash_erase_sector(addr);
	TEST_ASSERT_EQUAL(ESP_OK, ret);

	aspi_flash_wait_until_free();

	uint8_t data;

	ret = aspi_flash_read(addr, &data, 1);
	TEST_ASSERT_EQUAL(ESP_OK, ret);
	TEST_ASSERT_EQUAL(0xFF, data);

	ret = aspi_flash_deinit();
	TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("SPI_FLASH test erase chip", "[ASPIFLASH]")
{
	uint8_t data = 0xEF;

	const unsigned long long addr = 0x20;

	esp_err_t ret;

	if (aspi_flash_get_status() == ASPI_IDLE)
	{
		ret = aspi_flash_init();
		TEST_ASSERT_EQUAL(ESP_OK, ret);
	}
	ret = aspi_flash_erase_sector(addr);
	TEST_ASSERT_EQUAL(ESP_OK, ret);

	aspi_flash_wait_until_free();

	ret = aspi_flash_write(addr, &data, 1);
	TEST_ASSERT_EQUAL(ESP_OK, ret);

	aspi_flash_wait_until_free();

	uint8_t read_data;
	ret = aspi_flash_read(addr, &read_data, 1);
	TEST_ASSERT_EQUAL(ESP_OK, ret);
	TEST_ASSERT_EQUAL(data, read_data);

	ret = aspi_flash_erase_chip();
	TEST_ASSERT_EQUAL(ESP_OK, ret);

	aspi_flash_wait_until_free();

	ret = aspi_flash_read(addr, &read_data, 1);
	esp_err_t ret1 = aspi_flash_deinit();
	TEST_ASSERT_EQUAL(ESP_OK, ret);
	TEST_ASSERT_EQUAL(ESP_OK, ret1);
	TEST_ASSERT_EQUAL(0xFF, read_data);
}