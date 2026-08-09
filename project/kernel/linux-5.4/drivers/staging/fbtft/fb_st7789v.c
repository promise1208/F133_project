// SPDX-License-Identifier: GPL-2.0+
/*
 * FB driver for the ST7789V LCD Controller
 *
 * Copyright (C) 2015 Dennis Menschel
 */

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <video/mipi_display.h>

#include "fbtft.h"

#define DRVNAME "fb_st7789v"

#define DEFAULT_GAMMA \
	"70 2C 2E 15 10 09 48 33 53 0B 19 18 20 25\n" \
	"70 2C 2E 15 10 09 48 33 53 0B 19 18 20 25"
/*
panel@0 {
	compatible = "sitronix,st7789v";
	reg = <0>;
	width = <240>;
	height = <240>;
	regwidth = <8>;
	reset-gpios = <&pio PE 13 GPIO_ACTIVE_HIGH>;
	dc-gpios = <&pio PE 12 GPIO_ACTIVE_HIGH>;
	cs-gpios = <&pio PF 6 GPIO_ACTIVE_LOW>;
//	db-gpios = <&pio PC 4 GPIO_ACTIVE_HIGH>;
//	wr-gpios = <&pio PC 2 GPIO_ACTIVE_HIGH>;
	led-gpios = <&pio PE 7 GPIO_ACTIVE_LOW>;
	spi-max-frequency=<10000000>;
	spi-cpol;
	spi-cpha;
	rotate = <0>;
	rgb;
	fps = <30>;
	buswidth = <8>;
};
*/

#define HAREWARE_SPI 1
/**
 * enum st7789v_command - ST7789V display controller commands
 *
 * @PORCTRL: porch setting
 * @GCTRL: gate control
 * @VCOMS: VCOM setting
 * @VDVVRHEN: VDV and VRH command enable
 * @VRHS: VRH set
 * @VDVS: VDV set
 * @VCMOFSET: VCOM offset set
 * @PWCTRL1: power control 1
 * @PVGAMCTRL: positive voltage gamma control
 * @NVGAMCTRL: negative voltage gamma control
 *
 * The command names are the same as those found in the datasheet to ease
 * looking up their semantics and usage.
 *
 * Note that the ST7789V display controller offers quite a few more commands
 * which have been omitted from this list as they are not used at the moment.
 * Furthermore, commands that are compliant with the MIPI DCS have been left
 * out as well to avoid duplicate entries.
 */
enum st7789v_command {
	PORCTRL = 0xB2,
	GCTRL = 0xB7,
	VCOMS = 0xBB,
	VDVVRHEN = 0xC2,
	VRHS = 0xC3,
	VDVS = 0xC4,
	VCMOFSET = 0xC5,
	PWCTRL1 = 0xD0,
	PVGAMCTRL = 0xE0,
	NVGAMCTRL = 0xE1,
};

#define MADCTL_BGR BIT(3) /* bitmask for RGB/BGR order */
#define MADCTL_MV BIT(5) /* bitmask for page/column order */
#define MADCTL_MX BIT(6) /* bitmask for column address order */
#define MADCTL_MY BIT(7) /* bitmask for page address order */

#ifndef HAREWARE_SPI
#define DC(gpio, s) gpiod_set_value(gpio, s);
#define CS(gpio, s) gpiod_set_value(gpio, s);
#define MOSI(gpio, s) gpiod_set_value(gpio, s);
#define SCLK(gpio, s) gpiod_set_value(gpio, s);
static void sim_write_bytes(struct fbtft_par *par, u8 dat)
{
	u8 i;
	
	CS(par->gpio.cs, 0);
	for(i = 0; i < 8;i++) {
		SCLK(par->gpio.wr, 0);
		if(dat & 0x80)
			MOSI(par->gpio.db[0], 1)
	        else
			MOSI(par->gpio.db[0], 0)
		SCLK(par->gpio.wr, 1);
		dat<<=1;
	}
	CS(par->gpio.cs, 1);
}

static int software_sim_write(struct fbtft_par *par, void *buf, size_t len)
{
	char *buf_c = (char *)buf;
        while(len --) {
                sim_write_bytes(par, *buf_c ++);
	}
	return 0;
}
#else

static int hardware_spi_write(struct fbtft_par *par, void *buf, size_t len)
{
	struct spi_transfer t = {
		.tx_buf = buf,
		.len = len,
		.speed_hz = 10000000,
	};
	struct spi_message m;

	fbtft_par_dbg_hex(DEBUG_WRITE, par, par->info->device, u8, buf, len,
			  "%s(len=%zu): ", __func__, len);

	if (!par->spi) {
		dev_err(par->info->device,
			"%s: par->spi is unexpectedly NULL\n", __func__);
		return -1;
	}

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spi_sync(par->spi, &m);
}
#endif
static void set_addr_win(struct fbtft_par *par, int xs, int ys, int xe,
			 int ye)
{
	switch(par->info->var.rotate) {
	case 0:
		xs += 53;
		xe += 53;
		ys += 40;
		ye += 40;
	break;
	case 90:
		xs += 40;
		xe += 40;
		ys += 53;
		ye += 53;
	break;
	case 180:
		xs += 53;
		xe += 53;
		ys += 40;
		ye += 40;
	break;
	case 270:
		xs += 40;
		xe += 40;
		ys += 53;
		ye += 53;
	break;
	default:
	break;
	}

	write_reg(par, MIPI_DCS_SET_COLUMN_ADDRESS,
		  (xs >> 8) & 0xFF, xs & 0xFF, (xe >> 8) & 0xFF, xe & 0xFF);

	write_reg(par, MIPI_DCS_SET_PAGE_ADDRESS,
		  (ys >> 8) & 0xFF, ys & 0xFF, (ye >> 8) & 0xFF, ye & 0xFF);

	write_reg(par, MIPI_DCS_WRITE_MEMORY_START);
}

static int init_display(struct fbtft_par *par)
{
	gpiod_set_value(par->gpio.cs, 0);
	if(par->fbtftops.reset)
		par->fbtftops.reset(par);
	/* turn off sleep mode */
	write_reg(par, 0x11);
	mdelay(120);
	write_reg(par, 0x36, 0x00);
	write_reg(par, 0x3a, 0x05);
	write_reg(par, 0xb2, 0x0c, 0x0c, 0x00, 0x33, 0x33);

	write_reg(par, 0xb7, 0x35);
	write_reg(par, 0xbb, 0x19);
	write_reg(par, 0xc0, 0x2c);
	write_reg(par, 0xc2, 0x01);
	write_reg(par, 0xc3, 0x12);
	write_reg(par, 0xc4, 0x20);
	write_reg(par, 0xc6, 0x0f);
	write_reg(par, 0xd0, 0xa4, 0xa1);
	write_reg(par, 0xe0, 0xd0, 0x04, 0x0d, 0x11, 0x13, 0x2b, 0x3f, 0x54, 0x4c, 0x18, 0x0d, 0x0b, 0x1f, 0x23);
	write_reg(par, 0xe1, 0xd0, 0x04, 0x0c, 0x11, 0x13, 0x2c, 0x3f, 0x44, 0x51, 0x2f, 0x1f, 0x1f, 0x20, 0x23);
	write_reg(par, 0x21);
	write_reg(par, 0x29);

	return 0;
}

/**
 * set_var() - apply LCD properties like rotation and BGR mode
 *
 * @par: FBTFT parameter object
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static int set_var(struct fbtft_par *par)
{
	u8 madctl_par = 0;

	if (par->bgr)
		madctl_par |= MADCTL_BGR;
	switch (par->info->var.rotate) {
	case 0:
		break;
	case 90:
		madctl_par |= (MADCTL_MV | MADCTL_MY);
		break;
	case 180:
		madctl_par |= (MADCTL_MX | MADCTL_MY);
		break;
	case 270:
		madctl_par |= (MADCTL_MV | MADCTL_MX);
		break;
	default:
		return -EINVAL;
	}
	write_reg(par, MIPI_DCS_SET_ADDRESS_MODE, madctl_par);
	return 0;
}

/**
 * set_gamma() - set gamma curves
 *
 * @par: FBTFT parameter object
 * @curves: gamma curves
 *
 * Before the gamma curves are applied, they are preprocessed with a bitmask
 * to ensure syntactically correct input for the display controller.
 * This implies that the curves input parameter might be changed by this
 * function and that illegal gamma values are auto-corrected and not
 * reported as errors.
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static int set_gamma(struct fbtft_par *par, u32 *curves)
{
	int i;
	int j;
	int c; /* curve index offset */

	/*
	 * Bitmasks for gamma curve command parameters.
	 * The masks are the same for both positive and negative voltage
	 * gamma curves.
	 */
	static const u8 gamma_par_mask[] = {
		0xFF, /* V63[3:0], V0[3:0]*/
		0x3F, /* V1[5:0] */
		0x3F, /* V2[5:0] */
		0x1F, /* V4[4:0] */
		0x1F, /* V6[4:0] */
		0x3F, /* J0[1:0], V13[3:0] */
		0x7F, /* V20[6:0] */
		0x77, /* V36[2:0], V27[2:0] */
		0x7F, /* V43[6:0] */
		0x3F, /* J1[1:0], V50[3:0] */
		0x1F, /* V57[4:0] */
		0x1F, /* V59[4:0] */
		0x3F, /* V61[5:0] */
		0x3F, /* V62[5:0] */
	};

	for (i = 0; i < par->gamma.num_curves; i++) {
		c = i * par->gamma.num_values;
		for (j = 0; j < par->gamma.num_values; j++)
			curves[c + j] &= gamma_par_mask[j];
		write_reg(par, PVGAMCTRL + i,
			  curves[c + 0],  curves[c + 1],  curves[c + 2],
			  curves[c + 3],  curves[c + 4],  curves[c + 5],
			  curves[c + 6],  curves[c + 7],  curves[c + 8],
			  curves[c + 9],  curves[c + 10], curves[c + 11],
			  curves[c + 12], curves[c + 13]);
	}
	return 0;
}

/**
 * blank() - blank the display
 *
 * @par: FBTFT parameter object
 * @on: whether to enable or disable blanking the display
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static int blank(struct fbtft_par *par, bool on)
{
	if (on)
		write_reg(par, MIPI_DCS_SET_DISPLAY_OFF);
	else
		write_reg(par, MIPI_DCS_SET_DISPLAY_ON);
	return 0;
}

static void reset(struct fbtft_par *par)
{
	if (!par->gpio.reset)
		return;

	gpiod_set_value(par->gpio.reset, 0);
	mdelay(100);
	gpiod_set_value(par->gpio.reset, 1);
	mdelay(120);
}

static void write_reg8_bus8(struct fbtft_par *par, int len, ...)
{
	va_list args;
	int i, ret;
	u8 *buf = par->buf;

	if (unlikely(par->debug & DEBUG_WRITE_REGISTER)) {
		va_start(args, len);
		for (i = 0; i < len; i++)
			buf[i] = (u8)va_arg(args, unsigned int);
		va_end(args);
		fbtft_par_dbg_hex(DEBUG_WRITE_REGISTER, par, par->info->device,
				  u8, buf, len, "%s: ", __func__);
	}

	va_start(args, len);

	*buf = (u8)va_arg(args, unsigned int);
	if (par->gpio.dc)
		gpiod_set_value(par->gpio.dc, 0);
	ret = par->fbtftops.write(par, par->buf, sizeof(u8));
	if (ret < 0) {
		va_end(args);
		dev_err(par->info->device,
			"write() failed and returned %d\n", ret);
		return;
	}
	len--;

	if (par->gpio.dc)
		gpiod_set_value(par->gpio.dc, 1);

	if (len) {
		i = len;
		while (i--)
			*buf++ = (u8)va_arg(args, unsigned int);

		ret = par->fbtftops.write(par, par->buf, len * (sizeof(u8)));
		if (ret < 0) {
			va_end(args);
			dev_err(par->info->device,
				"write() failed and returned %d\n", ret);
			return;
		}
	}
	va_end(args);
}

static struct fbtft_display display = {
	.regwidth = 8,
	.width = 135,
	.height = 240,
	.gamma_num = 2,
	.gamma_len = 14,
	.gamma = DEFAULT_GAMMA,
	.fbtftops = {
		.init_display = init_display,
		.set_addr_win = set_addr_win,
		.write_register = write_reg8_bus8,
#ifdef HAREWARE_SPI
		.write = hardware_spi_write,
#else
		.write = software_sim_write,
#endif
		.reset = reset,
		.set_var = set_var,
		.set_gamma = set_gamma,
		.blank = blank,
	},
};

FBTFT_REGISTER_DRIVER(DRVNAME, "sitronix,st7789v", &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("platform:" DRVNAME);
MODULE_ALIAS("spi:st7789v");
MODULE_ALIAS("platform:st7789v");

MODULE_DESCRIPTION("FB driver for the ST7789V LCD Controller");
MODULE_AUTHOR("Dennis Menschel");
MODULE_LICENSE("GPL");
