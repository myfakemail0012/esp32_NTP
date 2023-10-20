/* test_mean.c: Implementation of a testable component.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "unity.h"
#include "app_spi.h"
#include "esp_err.h"
#include "inttypes.h"

TEST_CASE("SPI test init / deinit", "[init/deinit/pass]")
{
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
	const unsigned long long addr = 0x10;

	esp_err_t ret = aspi_init();
	TEST_ASSERT_EQUAL(ESP_OK, ret);

	ret = aspi_erase_sector(addr);
	TEST_ASSERT_EQUAL(ESP_OK, ret);

	ret = aspi_write(addr, (uint8_t *)txt, txt_len);
	TEST_ASSERT_EQUAL(ESP_OK, ret);

	char *read_buf[txt_len];
	ret = aspi_read(addr, (uint8_t *)read_buf, txt_len);
	TEST_ASSERT_EQUAL(ESP_OK, ret);
	TEST_ASSERT_EQUAL_STRING_LEN(txt, read_buf, txt_len);

	ret = aspi_erase_sector(addr);
	TEST_ASSERT_EQUAL(ESP_OK, ret);

	uint8_t data;

	ret = aspi_read(addr, &data, 1);
	TEST_ASSERT_EQUAL(ESP_OK, ret);
	TEST_ASSERT_EQUAL(0xFF, data);

	ret = aspi_deinit();
	TEST_ASSERT_EQUAL(ESP_OK, ret);
}