/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file init.c
 *
 * SAMV71-XULT-specific early startup code.
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "board_config.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <debug.h>
#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>

#include <nuttx/config.h>
#include <nuttx/board.h>
#include <nuttx/mmcsd.h>
#include <nuttx/mm/gran.h>
#include <chip.h>
#include <arch/board/board.h>
#include "arm_internal.h"

#include <px4_arch/io_timer.h>
#include <drivers/drv_hrt.h>
#include <drivers/drv_board_led.h>
#include <systemlib/px4_macros.h>
#include <px4_platform_common/init.h>
#include <px4_platform/gpio.h>
#include <px4_platform/board_dma_alloc.h>

# if defined(FLASH_BASED_PARAMS)
#  include <parameters/flashparams/flashfs.h>
#endif

/****************************************************************************
 * Pre-Processor Definitions
 ****************************************************************************/

__BEGIN_DECLS
extern void led_init(void);
extern void led_on(int led);
extern void led_off(int led);
__END_DECLS

/************************************************************************************
 * Name: board_peripheral_reset
 *
 * Description:
 *
 ************************************************************************************/
__EXPORT void board_peripheral_reset(int ms)
{
	UNUSED(ms);
}

/************************************************************************************
 * Name: board_on_reset
 *
 * Description:
 * Optionally provided function called on entry to board_system_reset
 * It should perform any house keeping prior to the rest.
 *
 * status - 1 if resetting to boot loader
 *          0 if just resetting
 *
 ************************************************************************************/
__EXPORT void board_on_reset(int status)
{
	/* No PWM channels configured yet for SAMV71 - TODO */
	(void)status;  /* Unused */
}

/************************************************************************************
 * Name: sam_boardinitialize
 *
 * Description:
 *   All SAM V71 architectures must provide the following entry point.  This entry
 *   point is called early in the initialization -- after all memory has been
 *   configured and mapped but before any devices have been initialized.
 *
 ************************************************************************************/

__EXPORT void
sam_boardinitialize(void)
{
	board_on_reset(-1); /* Reset PWM first thing */

	/* configure LEDs */
	board_autoled_initialize();

	/* Initialize and blink LED very early to show boot */
	led_init();
	for (int i = 0; i < 5; i++) {
		led_on(0);
		for (volatile int j = 0; j < 1000000; j++); // Simple delay
		led_off(0);
		for (volatile int j = 0; j < 1000000; j++);
	}

	/* Test syslog very early */
	syslog(LOG_ERR, "\n*** SAMV71 SAM_BOARDINITIALIZE START ***\n");

	/* configure pins */
	const uint32_t gpio[] = PX4_GPIO_INIT_LIST;
	px4_gpio_init(gpio, arraySize(gpio));

	/* configure USB interfaces */
	sam_usbinitialize();
}

/****************************************************************************
 * Name: board_app_initialize
 *
 * Description:
 *   Perform application specific initialization.  This function is never
 *   called directly from application code, but only indirectly via the
 *   (non-standard) boardctl() interface using the command BOARDIOC_INIT.
 *
 ****************************************************************************/

__EXPORT int board_app_initialize(uintptr_t arg)
{
	/* Blink LED 10 times FIRST to show we reached here */
	for (int i = 0; i < 10; i++) {
		led_on(0);
		up_mdelay(100);
		led_off(0);
		up_mdelay(100);
	}

	/* Test syslog early - before px4_platform_init */
	syslog(LOG_ERR, "\n\n*** SAMV71 BOARD_APP_INITIALIZE START ***\n");

	syslog(LOG_ERR, "[boot] Calling px4_platform_init...\n");
	px4_platform_init();
	syslog(LOG_ERR, "[boot] px4_platform_init completed\n");

	/* configure the DMA allocator */
	syslog(LOG_ERR, "[boot] Initializing DMA allocator...\n");
	if (board_dma_alloc_init() < 0) {
		syslog(LOG_ERR, "[boot] DMA alloc FAILED\n");
	} else {
		syslog(LOG_ERR, "[boot] DMA alloc OK\n");
	}

	syslog(LOG_ERR, "[boot] Starting LED driver...\n");
	drv_led_start();

	led_off(LED_RED);
	led_on(LED_GREEN); // Indicate Power
	led_off(LED_BLUE);

	syslog(LOG_ERR, "[boot] Initializing hardfault handler...\n");
	if (board_hardfault_init(2, true) != 0) {
		led_on(LED_RED);
		syslog(LOG_ERR, "[boot] Hardfault init FAILED\n");
	} else {
		syslog(LOG_ERR, "[boot] Hardfault init OK\n");
	}

#if defined(FLASH_BASED_PARAMS)
	static sector_descriptor_t params_sector_map[] = {
		{1, 128 * 1024, 0x00420000},
		{2, 128 * 1024, 0x00440000},
		{0, 0, 0},
	};

	/* Initialize the flashfs layer to use heap allocated memory */
	syslog(LOG_ERR, "[boot] Initializing flash params...\n");
	int result = parameter_flashfs_init(params_sector_map, NULL, 0);

	if (result != OK) {
		syslog(LOG_ERR, "[boot] FAILED to init params in FLASH %d\n", result);
		led_on(LED_AMBER);
	} else {
		syslog(LOG_ERR, "[boot] Flash params OK\n");
	}
#endif

	syslog(LOG_ERR, "[boot] board_app_initialize complete, returning OK\n");

	/* Test: blink LED after init completes */
	for (int i = 0; i < 3; i++) {
		syslog(LOG_ERR, "[boot] Post-init LED blink %d\n", i);
		led_on(0);
		up_mdelay(200);
		led_off(0);
		up_mdelay(200);
	}

	syslog(LOG_ERR, "[boot] About to return OK from board_app_initialize\n");

	/* Restore stdout to serial console for NSH
	 * px4_platform_init redirected it to console buffer, but NSH needs it on serial
	 */
	syslog(LOG_ERR, "[boot] Restoring stdout to serial console\n");
	int fd_console = open("/dev/console", O_WRONLY);
	if (fd_console >= 0) {
		dup2(fd_console, 1);  // Redirect stdout back to serial console
		close(fd_console);
		syslog(LOG_ERR, "[boot] stdout restored to /dev/console\n");
	} else {
		syslog(LOG_ERR, "[boot] Failed to open /dev/console: %d\n", errno);
	}

	/* Test console output directly */
	printf("\n\n");
	printf("===========================================\n");
	printf("SAMV71 Board Initialization Complete\n");
	printf("===========================================\n");
	printf("Serial console is working!\n");
	printf("Returning from board_app_initialize...\n");
	printf("NSH should start next.\n");
	printf("\n");
	fflush(stdout);

	return OK;
}
