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

TEST_CASE("SPI test write / read", "[read/write]")
{
	esp_err_t ret = aspi_init();
	TEST_ASSERT_EQUAL(ESP_OK, ret);

	ret = aspi_erase_chip();
	TEST_ASSERT_EQUAL(ESP_OK, ret);

	char *txt = "My test string";

	const size_t txt_len = strlen(txt);
	const unsigned long long addr = 0x10;

	ret = aspi_write(addr, (uint8_t *)txt, txt_len);
	TEST_ASSERT_EQUAL(ESP_OK, ret);

	char *read_buf[txt_len];
	ret = aspi_read(addr, (uint8_t *)read_buf, txt_len);
	TEST_ASSERT_EQUAL(ESP_OK, ret);
	TEST_ASSERT_EQUAL_STRING_LEN(txt, read_buf, txt_len);

	ret = aspi_erase_chip();
	TEST_ASSERT_EQUAL(ESP_OK, ret);

	uint8_t data;

	ret = aspi_read(addr, &data, 1);
	TEST_ASSERT_EQUAL(ESP_OK, ret);
	TEST_ASSERT_EQUAL(0xFF, data);

	ret = aspi_deinit();
	TEST_ASSERT_EQUAL(ESP_OK, ret);
}
// Expected 'My test string' Was 'Lh $ Sd `dring'