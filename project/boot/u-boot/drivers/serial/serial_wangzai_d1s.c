// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018 Anup Patel <anup@brainfault.org>
 */

#include <common.h>
#include <clk.h>
#include <debug_uart.h>
#include <dm.h>
#include <errno.h>
#include <fdtdec.h>
#include <log.h>
#include <watchdog.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <linux/compiler.h>
#include <serial.h>
#include <linux/err.h>

DECLARE_GLOBAL_DATA_PTR;

struct wangzai_uart_plat {
	uint32_t *regs;
};

#define SUNXI_UART_THR     0
#define SUNXI_UART_RBR     0
#define SUNXI_UART_USR     0x1F //addr:0x7C
#define SUNXI_UART_USR_NF  0x02
#define SUNXI_UART_USR_RFNE  0x04
#define SUNXI_UART_USR_RFFU  0x08

static int wangzai_serial_setbrg(struct udevice *dev, int baudrate)
{

	return 0;
}

static int wangzai_serial_probe(struct udevice *dev)
{
	return 0;
}

static int wangzai_serial_getc(struct udevice *dev)
{
	int c;
	struct wangzai_uart_plat *plat = dev_get_plat(dev);
	uint32_t *sunxi_uart = plat->regs;

	if ((sunxi_uart[SUNXI_UART_USR] & SUNXI_UART_USR_RFNE) != 0)
		c = sunxi_uart[SUNXI_UART_RBR];
	else
		c = -1;

	return c;
}

static int wangzai_serial_putc(struct udevice *dev, const char ch)
{
	int rc = 0;
	struct wangzai_uart_plat *plat = dev_get_plat(dev);
	uint32_t *sunxi_uart = plat->regs;

	while ((sunxi_uart[SUNXI_UART_USR] & SUNXI_UART_USR_NF) == 0) {
		asm("nop");
	}
	sunxi_uart[SUNXI_UART_THR] = ch;

	return rc;
}

static int wangzai_serial_pending(struct udevice *dev, bool input)
{
	struct wangzai_uart_plat *plat = dev_get_plat(dev);
	uint32_t *sunxi_uart = plat->regs;

	if (input)
		return (sunxi_uart[SUNXI_UART_USR] & SUNXI_UART_USR_RFFU) ? 1 : 0;
        else
                return (sunxi_uart[SUNXI_UART_USR] & SUNXI_UART_USR_NF) ? 0 : 1;

}

static int wangzai_serial_of_to_plat(struct udevice *dev)
{
	struct wangzai_uart_plat *plat = dev_get_plat(dev);

	plat->regs = (uint32_t *)(uintptr_t)dev_read_addr(dev);
	if (IS_ERR(plat->regs))
		return PTR_ERR(plat->regs);

	return 0;
}

static const struct dm_serial_ops wangzai_serial_ops = {
	.putc = wangzai_serial_putc,
	.getc = wangzai_serial_getc,
	.pending = wangzai_serial_pending,
};

static const struct udevice_id wangzai_serial_ids[] = {
	{ .compatible = "dawn,uart0" },
	{ }
};

U_BOOT_DRIVER(serial_wangzai) = {
	.name	= "serial_wangzai",
	.id	= UCLASS_SERIAL,
	.of_match = wangzai_serial_ids,
	.of_to_plat = wangzai_serial_of_to_plat,
	.plat_auto	= sizeof(struct wangzai_uart_plat),
	.probe = wangzai_serial_probe,
	.ops	= &wangzai_serial_ops,
};


