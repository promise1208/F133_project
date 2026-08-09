/*
 * (C) Copyright 2018
* SPDX-License-Identifier:	GPL-2.0+
 * wangwei <wangwei@allwinnertech.com>
 */

#include <common.h>
#include <private_boot0.h>
#include <private_uboot.h>
#include <private_toc.h>
#include <arch/clock.h>
#include <arch/uart.h>
#include <arch/dram.h>
#include <arch/rtc.h>
#include <arch/gpio.h>
#ifdef CFG_DDR_SOFT_TRAIN
#include <arch/efuse.h>
#endif

#define JUMP_ADDR (0x40000000)
#define DTB_ADDR (0x42200000)
static int boot0_clear_env(void);

#define SDNUM 3
l_sd_f sd[SDNUM] = {
	{ //opensbi
		.start_sector = 200,
		.sector_num = 300,
		.buf = (void *)0x40000000,
	},
	{ //dtb
		.start_sector = 500,
		.sector_num = 100,
		.buf = (void *)0x42200000,
	},
	{ //uboot
		.start_sector = 600,
		.sector_num = 2000,
		.buf = (void *)0x40200000,
	},
};

void main(void)
{
	int dram_size;
	int status;

	sunxi_serial_init(BT0_head.prvt_head.uart_port, (void *)BT0_head.prvt_head.uart_ctrl, 6);
	printf("HELLO! BOOT0 is starting! compile time = %s\n", __TIME__);
	printf("BOOT0 commit : %s\n", BT0_head.hash);
	sunxi_set_printf_debug_mode(BT0_head.prvt_head.debug_mode);

	status = sunxi_board_init();
	if(status)
		goto _BOOT_ERROR;

	if (rtc_probe_fel_flag()) {
		rtc_clear_fel_flag();
		goto _BOOT_ERROR;
#ifdef CFG_SUNXI_PHY_KEY
	} else if (check_update_key(&key_input)) {
		goto _BOOT_ERROR;
#endif
	} else if (BT0_head.prvt_head.enable_jtag) {
		printf("enable_jtag\n");
		boot_set_gpio((normal_gpio_cfg *)BT0_head.prvt_head.jtag_gpio, 5, 1);
	}

#ifdef FPGA_PLATFORM
	dram_size = mctl_init((void *)BT0_head.prvt_head.dram_para);
#else
#ifdef CFG_DDR_SOFT_TRAIN
	if (BT0_head.prvt_head.dram_para[30] & (1 << 11))
		neon_enable();
#endif
	dram_size = init_DRAM(0, (void *)BT0_head.prvt_head.dram_para);
#endif
	if(!dram_size)
		goto _BOOT_ERROR;
	else {
		printf("dram size =%d\n", dram_size);
	}

	char uart_input_value = get_uart_input();

	if (uart_input_value == '2') {
		sunxi_set_printf_debug_mode(3);
		printf("detected user input 2\n");
		goto _BOOT_ERROR;
	} else if (uart_input_value == 'd') {
		sunxi_set_printf_debug_mode(8);
		printf("detected user input d\n");
	}

	mmu_enable(dram_size);
	malloc_init(CONFIG_HEAP_BASE, CONFIG_HEAP_SIZE);
	status = sunxi_board_late_init();
	if (status)
		goto _BOOT_ERROR;

	status = load_package((void *)sd, SDNUM);
	if (status)
		goto _BOOT_ERROR;

	mmu_disable( );

	printf("Jump to second Boot.\n");

	boot0_jmp_opensbi(0, DTB_ADDR, JUMP_ADDR);

_BOOT_ERROR:
	printf("Boot Error\n");
	boot0_clear_env();
	boot0_jmp(FEL_BASE);

}

static int boot0_clear_env(void)
{
	sunxi_board_exit();
	sunxi_board_clock_reset();
	mmu_disable();
	mdelay(10);

	return 0;
}
