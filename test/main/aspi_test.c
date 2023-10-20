
#include <stdio.h>
#include <string.h>
#include "unity.h"

static void print_banner(const char *text);

void app_main(void)
{
	/* These are the different ways of running registered tests.
	 * In practice, only one of them is usually needed.
	 *
	 * UNITY_BEGIN() and UNITY_END() calls tell Unity to print a summary
	 * (number of tests executed/failed/ignored) of tests executed between these calls.
	 */
	print_banner("Executing SPI test init / deinit");
	UNITY_BEGIN();
	unity_run_test_by_name("SPI test init / deinit");
	UNITY_END();

	print_banner("Executing SPI test read null buffer");
	UNITY_BEGIN();
	unity_run_test_by_name("SPI test read null buffer");
	UNITY_END();

	print_banner("Executing SPI test read out of range");
	UNITY_BEGIN();
	unity_run_test_by_name("SPI test read out of range");
	UNITY_END();

	print_banner("Executing SPI test write null buffer");
	UNITY_BEGIN();
	unity_run_test_by_name("SPI test write null buffer / read");
	UNITY_END();

	print_banner("Executing SPI test write out of range");
	UNITY_BEGIN();
	unity_run_test_by_name("SPI test write out of range");
	UNITY_END();

	print_banner("Executing SPI test write / read / erase");
	UNITY_BEGIN();
	unity_run_test_by_name("SPI test write / read / erase");
	UNITY_END();

	print_banner("Running all the registered tests");
	UNITY_BEGIN();
	unity_run_all_tests();
	UNITY_END();

	print_banner("Starting interactive test menu");
	/* This function will not return, and will be busy waiting for UART input.
	 * Make sure that task watchdog is disabled if you use this function.
	 */
	unity_run_menu();
}

static void print_banner(const char *text)
{
	printf("\n#### %s #####\n\n", text);
}
