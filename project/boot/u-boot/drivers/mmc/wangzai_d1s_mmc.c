// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2007-2011
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Aaron <leafy.myeh@allwinnertech.com>
 *
 * MMC driver for allwinner sunxi platform.
 *
 * This driver is used by the (ARM) SPL with the legacy MMC interface, and
 * by U-Boot proper using the full DM interface. The actual hardware access
 * code is common, and comes first in this file.
 * The legacy MMC interface implementation comes next, followed by the
 * proper DM_MMC implementation at the end.
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <log.h>
#include <malloc.h>
#include <mmc.h>
#include <clk.h>
#include <reset.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/types.h>

#define CCM_PLL4_CTRL_N_SHIFT		8
#define CCM_PLL4_CTRL_N_MASK		(0xff << CCM_PLL4_CTRL_N_SHIFT)
#define CCM_PLL4_CTRL_P_SHIFT		16
#define CCM_PLL4_CTRL_P_MASK		(0x1 << CCM_PLL4_CTRL_P_SHIFT)
#define CCM_PLL4_CTRL_M_SHIFT		18
#define CCM_PLL4_CTRL_M_MASK		(0x1 << CCM_PLL4_CTRL_M_SHIFT)

/* pllx_cfg bits */
#define CCM_PLL1_CTRL_N(n)		(((n) & 0xff) << 8)
#define CCM_PLL1_CTRL_P(n)		(((n) & 0x1) << 16)
#define CCM_PLL1_CTRL_EN		(1 << 31)
#define CCM_PLL1_CLOCK_TIME_2		(2 << 24)

#define CCM_PLL2_CTRL_N(n)		(((n) & 0xff) << 8)
#define CCM_PLL2_CTRL_P(n)		(((n) & 0x1) << 16)
#define CCM_PLL2_CTRL_EN		(1 << 31)
#define CCM_PLL2_CLOCK_TIME_2		(2 << 24)

#define CCM_PLL4_CTRL_N(n)		(((n) & 0xff) << 8)
#define CCM_PLL4_CTRL_EN		(1 << 31)

#define CCM_PLL6_CTRL_N(n)		(((n) & 0xff) << 8)
#define CCM_PLL6_CTRL_P(p)		(((p) & 0x1) << 16)
#define CCM_PLL6_CTRL_EN		(1 << 31)
#define CCM_PLL6_CFG_UPDATE             (1 << 30)

#define CCM_PLL12_CTRL_N(n)		(((n) & 0xff) << 8)
#define CCM_PLL12_CTRL_EN		(1 << 31)

#define PLL_C0CPUX_STATUS               (1 << 0)
#define PLL_C1CPUX_STATUS               (1 << 1)
#define PLL_DDR_STATUS                  (1 << 5)
#define PLL_PERIPH1_STATUS              (1 << 11)

/* cpu_clk_source bits */
#define C0_CPUX_CLK_SRC_SHIFT           0
#define C1_CPUX_CLK_SRC_SHIFT           8
#define C0_CPUX_CLK_SRC_MASK            (1 << C0_CPUX_CLK_SRC_SHIFT)
#define C1_CPUX_CLK_SRC_MASK            (1 << C1_CPUX_CLK_SRC_SHIFT)
#define C0_CPUX_CLK_SRC_OSC24M		(0 << C0_CPUX_CLK_SRC_SHIFT)
#define C0_CPUX_CLK_SRC_PLL1		(1 << C0_CPUX_CLK_SRC_SHIFT)
#define C1_CPUX_CLK_SRC_OSC24M		(0 << C1_CPUX_CLK_SRC_SHIFT)
#define C1_CPUX_CLK_SRC_PLL2		(1 << C1_CPUX_CLK_SRC_SHIFT)

/* c0_cfg */
#define C0_CFG_AXI0_CLK_DIV_RATIO(n)    (((n - 1) & 0x3) << 0)
#define C0_CFG_APB0_CLK_DIV_RATIO(n)    (((n - 1) & 0x3) << 8)

/* ahbx_cfg */
#define AHBx_SRC_CLK_SELECT_SHIFT       24
#define AHBx_SRC_MASK                   (0x3 << AHBx_SRC_CLK_SELECT_SHIFT)
#define AHB0_SRC_GTBUS_CLK              (0x0 << AHBx_SRC_CLK_SELECT_SHIFT)
#define AHB1_SRC_GTBUS_CLK              (0x0 << AHBx_SRC_CLK_SELECT_SHIFT)
#define AHB2_SRC_OSC24M                 (0x0 << AHBx_SRC_CLK_SELECT_SHIFT)
#define AHBx_SRC_PLL_PERIPH0            (0x1 << AHBx_SRC_CLK_SELECT_SHIFT)
#define AHBx_SRC_PLL_PERIPH1            (0x2 << AHBx_SRC_CLK_SELECT_SHIFT)
#define AHBx_CLK_DIV_RATIO(n)           (((ffs(n) - 1) & 0x3) << 0)

/* apb0_cfg */
#define APB0_SRC_CLK_SELECT_SHIFT       24
#define APB0_SRC_MASK                   (0x1 << APB0_SRC_CLK_SELECT_SHIFT)
#define APB0_SRC_OSC24M                 (0x0 << APB0_SRC_CLK_SELECT_SHIFT)
#define APB0_SRC_PLL_PERIPH0            (0x1 << APB0_SRC_CLK_SELECT_SHIFT)
#define APB0_CLK_DIV_RATIO(n)           (((ffs(n) - 1) & 0x3) << 0)

/* gtbus_clk_cfg */
#define GTBUS_SRC_CLK_SELECT_SHIFT      24
#define GTBUS_SRC_MASK                  (0x3 << GTBUS_SRC_CLK_SELECT_SHIFT)
#define GTBUS_SRC_OSC24M                (0x0 << GTBUS_SRC_CLK_SELECT_SHIFT)
#define GTBUS_SRC_PLL_PERIPH0           (0x1 << GTBUS_SRC_CLK_SELECT_SHIFT)
#define GTBUS_SRC_PLL_PERIPH1           (0x2 << GTBUS_SRC_CLK_SELECT_SHIFT)
#define GTBUS_CLK_DIV_RATIO(n)          (((n - 1) & 0x3) << 0)

/* cci400_clk_cfg */
#define CCI400_SRC_CLK_SELECT_SHIFT     24
#define CCI400_SRC_MASK                 (0x3 << CCI400_SRC_CLK_SELECT_SHIFT)
#define CCI400_SRC_OSC24M               (0x0 << CCI400_SRC_CLK_SELECT_SHIFT)
#define CCI400_SRC_PLL_PERIPH0          (0x1 << CCI400_SRC_CLK_SELECT_SHIFT)
#define CCI400_SRC_PLL_PERIPH1          (0x2 << CCI400_SRC_CLK_SELECT_SHIFT)
#define CCI400_CLK_DIV_RATIO(n)         (((n - 1) & 0x3) << 0)

/* sd#_clk_cfg fields */
#define CCM_MMC_CTRL_M(x)		((x) - 1)
#define CCM_MMC_CTRL_OCLK_DLY(x)	((x) << 8)
#define CCM_MMC_CTRL_N(x)		((x) << 16)
#define CCM_MMC_CTRL_SCLK_DLY(x)	((x) << 20)
#define CCM_MMC_CTRL_OSCM24		(0 << 24)
#define CCM_MMC_CTRL_PLL_PERIPH0	(1 << 24)
#define CCM_MMC_CTRL_ENABLE		(1 << 31)

/* ahb_gate0 fields */
#define AHB_GATE_OFFSET_MCTL		14

/* On sun9i all sdc-s share their ahb gate, so ignore (x) */
#define AHB_GATE_OFFSET_NAND0		13
#define AHB_GATE_OFFSET_MMC(x)		8

/* ahb gate1 field */
#define AHB_GATE_OFFSET_DMA		24

/* apb1_gate fields */
#define APB1_GATE_UART_SHIFT		16
#define APB1_GATE_UART_MASK		(0xff << APB1_GATE_UART_SHIFT)
#define APB1_GATE_TWI_SHIFT		0
#define APB1_GATE_TWI_MASK		(0xf << APB1_GATE_TWI_SHIFT)

/* ahb_reset0_cfg fields */
#define AHB_RESET_OFFSET_MCTL		14

/* On sun9i all sdc-s share their ahb reset, so ignore (x) */
#define AHB_RESET_OFFSET_MMC(x)		8

/* apb1_reset_cfg fields */
#define APB1_RESET_UART_SHIFT		16
#define APB1_RESET_UART_MASK		(0xff << APB1_RESET_UART_SHIFT)
#define APB1_RESET_TWI_SHIFT		0
#define APB1_RESET_TWI_MASK		(0xf << APB1_RESET_TWI_SHIFT)



#ifndef CCM_MMC_CTRL_MODE_SEL_NEW
#define CCM_MMC_CTRL_MODE_SEL_NEW	0
#endif
#define L1_CACHE_BYTES (64)
#define PT_TO_U(p)   ((phys_addr_t)(p))
#define WR_MB() 	wmb()
//#define wmb()		RISCV_FENCE(ow, ow)

struct sunxi_mmc {
	u32 gctrl;		/* 0x00 global control */
	u32 clkcr;		/* 0x04 clock control */
	u32 timeout;		/* 0x08 time out */
	u32 width;		/* 0x0c bus width */
	u32 blksz;		/* 0x10 block size */
	u32 bytecnt;		/* 0x14 byte count */
	u32 cmd;		/* 0x18 command */
	u32 arg;		/* 0x1c argument */
	u32 resp0;		/* 0x20 response 0 */
	u32 resp1;		/* 0x24 response 1 */
	u32 resp2;		/* 0x28 response 2 */
	u32 resp3;		/* 0x2c response 3 */
	u32 imask;		/* 0x30 interrupt mask */
	u32 mint;		/* 0x34 masked interrupt status */
	u32 rint;		/* 0x38 raw interrupt status */
	u32 status;		/* 0x3c status */
	u32 ftrglevel;		/* 0x40 FIFO threshold watermark*/
	u32 funcsel;		/* 0x44 function select */
	u32 cbcr;		/* 0x48 CIU byte count */
	u32 bbcr;		/* 0x4c BIU byte count */
	u32 debugc;		/* 0x50 debug enable */
	u32 res0;		/* 0x54 reserved */
	u32 a12a;		/* 0x58 Auto command 12 argument */
	u32 ntsr;		/* 0x5c	New timing set register */
	u32 res1[8];
	u32 dmac;		/* 0x80 internal DMA control */
	u32 dlba;		/* 0x84 internal DMA descr list base address */
	u32 idst;		/* 0x88 internal DMA status */
	u32 idie;		/* 0x8c internal DMA interrupt enable */
	u32 chda;		/* 0x90 */
	u32 cbda;		/* 0x94 */
	u32 res2[26];
	u32 res3[17];
	u32 samp_dl;
	u32 res4[46];
	u32 fifo;		/* 0x100 / 0x200 FIFO access address */
};

#define SUNXI_MMC_CLK_POWERSAVE			(0x1 << 17)
#define SUNXI_MMC_CLK_ENABLE			(0x1 << 16)
#define SUNXI_MMC_CLK_DIVIDER_MASK		(0xff)

#define SUNXI_MMC_GCTRL_SOFT_RESET		(0x1 << 0)
#define SUNXI_MMC_GCTRL_FIFO_RESET		(0x1 << 1)
#define SUNXI_MMC_GCTRL_DMA_RESET		(0x1 << 2)
#define SUNXI_MMC_GCTRL_RESET			(SUNXI_MMC_GCTRL_SOFT_RESET|\
						SUNXI_MMC_GCTRL_FIFO_RESET|\
						SUNXI_MMC_GCTRL_DMA_RESET)
#define SUNXI_MMC_GCTRL_DMA_ENABLE		(0x1 << 5)
#define SUNXI_MMC_GCTRL_ACCESS_BY_AHB   	(0x1 << 31)

#define SUNXI_MMC_CMD_RESP_EXPIRE		(0x1 << 6)
#define SUNXI_MMC_CMD_LONG_RESPONSE		(0x1 << 7)
#define SUNXI_MMC_CMD_CHK_RESPONSE_CRC		(0x1 << 8)
#define SUNXI_MMC_CMD_DATA_EXPIRE		(0x1 << 9)
#define SUNXI_MMC_CMD_WRITE			(0x1 << 10)
#define SUNXI_MMC_CMD_AUTO_STOP			(0x1 << 12)
#define SUNXI_MMC_CMD_WAIT_PRE_OVER		(0x1 << 13)
#define SUNXI_MMC_CMD_SEND_INIT_SEQ		(0x1 << 15)
#define SUNXI_MMC_CMD_UPCLK_ONLY		(0x1 << 21)
#define SUNXI_MMC_CMD_START			(0x1 << 31)

#define SUNXI_MMC_RINT_RESP_ERROR		(0x1 << 1)
#define SUNXI_MMC_RINT_COMMAND_DONE		(0x1 << 2)
#define SUNXI_MMC_RINT_DATA_OVER		(0x1 << 3)
#define SUNXI_MMC_RINT_TX_DATA_REQUEST		(0x1 << 4)
#define SUNXI_MMC_RINT_RX_DATA_REQUEST		(0x1 << 5)
#define SUNXI_MMC_RINT_RESP_CRC_ERROR		(0x1 << 6)
#define SUNXI_MMC_RINT_DATA_CRC_ERROR		(0x1 << 7)
#define SUNXI_MMC_RINT_RESP_TIMEOUT		(0x1 << 8)
#define SUNXI_MMC_RINT_DATA_TIMEOUT		(0x1 << 9)
#define SUNXI_MMC_RINT_VOLTAGE_CHANGE_DONE	(0x1 << 10)
#define SUNXI_MMC_RINT_FIFO_RUN_ERROR		(0x1 << 11)
#define SUNXI_MMC_RINT_HARD_WARE_LOCKED		(0x1 << 12)
#define SUNXI_MMC_RINT_START_BIT_ERROR		(0x1 << 13)
#define SUNXI_MMC_RINT_AUTO_COMMAND_DONE	(0x1 << 14)
#define SUNXI_MMC_RINT_END_BIT_ERROR		(0x1 << 15)
#define SUNXI_MMC_RINT_SDIO_INTERRUPT		(0x1 << 16)
#define SUNXI_MMC_RINT_CARD_INSERT		(0x1 << 30)
#define SUNXI_MMC_RINT_CARD_REMOVE		(0x1 << 31)
#define SUNXI_MMC_RINT_INTERRUPT_ERROR_BIT      \
	(SUNXI_MMC_RINT_RESP_ERROR |		\
	 SUNXI_MMC_RINT_RESP_CRC_ERROR |	\
	 SUNXI_MMC_RINT_DATA_CRC_ERROR |	\
	 SUNXI_MMC_RINT_RESP_TIMEOUT |		\
	 SUNXI_MMC_RINT_DATA_TIMEOUT |		\
	 SUNXI_MMC_RINT_VOLTAGE_CHANGE_DONE |	\
	 SUNXI_MMC_RINT_FIFO_RUN_ERROR |	\
	 SUNXI_MMC_RINT_HARD_WARE_LOCKED |	\
	 SUNXI_MMC_RINT_START_BIT_ERROR |	\
	 SUNXI_MMC_RINT_END_BIT_ERROR) /* 0xbfc2 */
#define SUNXI_MMC_RINT_INTERRUPT_DONE_BIT	\
	(SUNXI_MMC_RINT_AUTO_COMMAND_DONE |	\
	 SUNXI_MMC_RINT_DATA_OVER |		\
	 SUNXI_MMC_RINT_COMMAND_DONE |		\
	 SUNXI_MMC_RINT_VOLTAGE_CHANGE_DONE)

#define SUNXI_MMC_STATUS_RXWL_FLAG		(0x1 << 0)
#define SUNXI_MMC_STATUS_TXWL_FLAG		(0x1 << 1)
#define SUNXI_MMC_STATUS_FIFO_EMPTY		(0x1 << 2)
#define SUNXI_MMC_STATUS_FIFO_FULL		(0x1 << 3)
#define SUNXI_MMC_STATUS_CARD_PRESENT		(0x1 << 8)
#define SUNXI_MMC_STATUS_CARD_DATA_BUSY		(0x1 << 9)
#define SUNXI_MMC_STATUS_DATA_FSM_BUSY		(0x1 << 10)
#define SUNXI_MMC_STATUS_FIFO_LEVEL(reg)	(((reg) >> 17) & 0x3fff)

#define SUNXI_MMC_NTSR_MODE_SEL_NEW		(0x1 << 31)

#define SUNXI_MMC_IDMAC_RESET			(0x1 << 0)
#define SUNXI_MMC_IDMAC_FIXBURST		(0x1 << 1)
#define SUNXI_MMC_IDMAC_ENABLE			(0x1 << 7)

#define SUNXI_MMC_IDIE_TXIRQ			(0x1 << 0)
#define SUNXI_MMC_IDIE_RXIRQ			(0x1 << 1)

#define SUNXI_MMC_COMMON_CLK_GATE		(1 << 16)
#define SUNXI_MMC_COMMON_RESET			(1 << 18)

#define SUNXI_MMC_CAL_DL_SW_EN			(0x1 << 7)
#define MMC_DMA_MINALIGN			(32)


struct sunxi_mmc_des {
	u32:1, dic:1,           /* disable interrupt on completion */
	last_des:1,             /* 1-this data buffer is the last buffer */
	first_des:1,            /* 1-data buffer is the first buffer,
	                           0-data buffer contained in the next descriptor is 1st buffer */
	des_chain:1,            /* 1-the 2nd address in the descriptor is the next descriptor address */
	end_of_ring:1,          /* 1-last descriptor flag when using dual data buffer in descriptor */
	: 24,
	card_err_sum:1, /* transfer error flag */
	own:1;                  /* des owner:1-idma owns it, 0-host owns it */

#define SDXC_DES_NUM_SHIFT 12
#define SDXC_DES_BUFFER_MAX_LEN (1 << SDXC_DES_NUM_SHIFT)
	u32 data_buf1_sz:16, data_buf2_sz:16;
	
	u32 buf_addr_ptr1;
	u32 buf_addr_ptr2;
};

struct sunxi_mmc_plat {
	struct mmc_config cfg;
	struct mmc mmc;
};

struct sunxi_mmc_priv {
	unsigned mmc_no;
	uint32_t *mclkreg;
	unsigned fatal_err;
	struct gpio_desc cd_gpio;	/* Change Detect GPIO */
	struct sunxi_mmc *reg;
	struct mmc_config cfg;
};

static void flush_dcache_range(unsigned long start, unsigned long end)
{
	register unsigned long i asm("a0") = start & ~(L1_CACHE_BYTES - 1);
	for (; i < end; i += L1_CACHE_BYTES)
		asm volatile(".long 0x0295000b");       /*dcache.cpa a0*/
	asm volatile(".long 0x01b0000b");               /*sync.is*/
}

static void invalidate_dcache_range(unsigned long start, unsigned long end)
{
	register unsigned long i asm("a0") = start & ~(L1_CACHE_BYTES - 1);
	for (; i < end; i += L1_CACHE_BYTES)
		asm volatile(".long 0x02a5000b");       /*dcache.ipa a0*/
	asm volatile(".long 0x01b0000b");               /*sync.is*/
}
#define  OSAL_CacheRangeFlush(__s, __l, __a)  flush_dcache_range(PT_TO_U(__s), PT_TO_U(__s)+__l - 1)
#define  OSAL_CacheRangeInvaild(__s, __l, __a)  invalidate_dcache_range(PT_TO_U(__s), PT_TO_U(__s)+__l - 1)

static int mmc_set_mod_clk(struct sunxi_mmc_priv *priv, unsigned int hz)
{
	unsigned int pll, pll_hz, div, n;
	u32 val = 0;

	if (hz <= 24000000) {
		pll = CCM_MMC_CTRL_OSCM24;
		pll_hz = 24000000;
	}

	div = pll_hz / hz;
	if (pll_hz % hz)
		div++;

	n = 0;
	while (div > 16) {
		n++;
		div = (div + 1) / 2;
	}

	if (n > 3) {
		printf("mmc %u error cannot set clock to %u\n", priv->mmc_no,
		       hz);
		return -1;
	}

	writel(CCM_MMC_CTRL_ENABLE| pll | CCM_MMC_CTRL_N(n) |
	       CCM_MMC_CTRL_M(div) | val, priv->mclkreg);

	debug("mmc %u set mod-clk req %u parent %u n %u m %u rate %u\n",
	      priv->mmc_no, hz, pll_hz, 1u << n, div, pll_hz / (1u << n) / div);

	return 0;
}

static int mmc_update_clk(struct sunxi_mmc_priv *priv)
{
	unsigned int cmd;
	unsigned timeout_msecs = 2000;
	unsigned long start = get_timer(0);

	cmd = SUNXI_MMC_CMD_START |
	      SUNXI_MMC_CMD_UPCLK_ONLY |
	      SUNXI_MMC_CMD_WAIT_PRE_OVER;

	writel(cmd, &priv->reg->cmd);
	while (readl(&priv->reg->cmd) & SUNXI_MMC_CMD_START) {
		if (get_timer(start) > timeout_msecs)
			return -1;
	}

	/* clock update sets various irq status bits, clear these */
	writel(readl(&priv->reg->rint), &priv->reg->rint);

	return 0;
}

static int mmc_config_clock(struct sunxi_mmc_priv *priv, struct mmc *mmc)
{
	unsigned rval = readl(&priv->reg->clkcr);

	/* Disable Clock */
	rval &= ~SUNXI_MMC_CLK_ENABLE;
	writel(rval, &priv->reg->clkcr);
	if (mmc_update_clk(priv))
		return -1;

	/* Set mod_clk to new rate */
	if (mmc_set_mod_clk(priv, mmc->clock))
		return -1;

	/* Clear internal divider */
	rval &= ~SUNXI_MMC_CLK_DIVIDER_MASK;
	writel(rval, &priv->reg->clkcr);

#if 1
	writel(SUNXI_MMC_CAL_DL_SW_EN, &priv->reg->samp_dl);
#endif

	/* Re-enable Clock */
	rval |= SUNXI_MMC_CLK_ENABLE;
	writel(rval, &priv->reg->clkcr);
	if (mmc_update_clk(priv))
		return -1;

	return 0;
}

static int sunxi_mmc_set_ios_common(struct sunxi_mmc_priv *priv,
				    struct mmc *mmc)
{
	debug("set ios: bus_width: %x, clock: %d\n",
	      mmc->bus_width, mmc->clock);

	/* Change clock first */
	if (mmc->clock && mmc_config_clock(priv, mmc) != 0) {
		priv->fatal_err = 1;
		return -EINVAL;
	}

	/* Change bus width */
	if (mmc->bus_width == 8)
		writel(0x2, &priv->reg->width);
	else if (mmc->bus_width == 4)
		writel(0x1, &priv->reg->width);
	else
		writel(0x0, &priv->reg->width);

	return 0;
}

static int mmc_trans_data_by_dma(struct sunxi_mmc_priv *priv, struct mmc *mmc,
				 struct mmc_data *data, struct sunxi_mmc_des *pdes)

{
	unsigned byte_cnt	      = data->blocksize * data->blocks;
	unsigned char *buff;
	unsigned des_idx       = 0;
	unsigned buff_frag_num = 0;
	unsigned remain;
	unsigned i, rval;
	u32 timeout = 0;

	buff = data->flags & MMC_DATA_READ ? (unsigned char *)data->dest :
					     (unsigned char *)data->src;

	buff_frag_num = byte_cnt >> SDXC_DES_NUM_SHIFT;
	remain	= byte_cnt & (SDXC_DES_BUFFER_MAX_LEN - 1);
	if (remain)
		buff_frag_num++;
	else
		remain = SDXC_DES_BUFFER_MAX_LEN;

	if (data->flags & MMC_DATA_WRITE)
		OSAL_CacheRangeFlush(buff, (unsigned long)byte_cnt, 0);

	for (i = 0; i < buff_frag_num; i++, des_idx++) {
		memset((void *)&pdes[des_idx], 0, sizeof(struct sunxi_mmc_des));
		pdes[des_idx].des_chain = 1;
		pdes[des_idx].own       = 1;
		pdes[des_idx].dic       = 1;
		if (buff_frag_num > 1 && i != buff_frag_num - 1) {
			pdes[des_idx].data_buf1_sz = SDXC_DES_BUFFER_MAX_LEN;
		} else
			pdes[des_idx].data_buf1_sz = remain;

		pdes[des_idx].buf_addr_ptr1 =
			((size_t)buff + i * SDXC_DES_BUFFER_MAX_LEN) >> 2;
		if (i == 0)
			pdes[des_idx].first_des = 1;

		if (i == buff_frag_num - 1) {
			pdes[des_idx].dic	   = 0;
			pdes[des_idx].last_des      = 1;
			pdes[des_idx].end_of_ring   = 1;
			pdes[des_idx].buf_addr_ptr2 = 0;
		} else {
			pdes[des_idx].buf_addr_ptr2 =
				((size_t)&pdes[des_idx + 1]) >> 2;
		}
	}
	OSAL_CacheRangeFlush(pdes, sizeof(struct sunxi_mmc_des) * (des_idx+1), 0);
	WR_MB();
	/*
	 * GCTRLREG
	 * GCTRL[2]     : DMA reset
	 * GCTRL[5]     : DMA enable
	 *
	 * IDMACREG
	 * IDMAC[0]     : IDMA soft reset
	 * IDMAC[1]     : IDMA fix burst flag
	 * IDMAC[7]     : IDMA on
	 *
	 * IDIECREG
	 * IDIE[0]      : IDMA transmit interrupt flag
	 * IDIE[1]      : IDMA receive interrupt flag
	 */
	rval = readl(&priv->reg->gctrl);
	writel(rval | (1 << 5) | (1 << 2),
	       &priv->reg->gctrl); /* dma enable */
	timeout = timer_get_us() + 0xffff;
	while (readl(&priv->reg->gctrl) & (1 << 2)) {
		if (timer_get_us() > timeout) {
			printf("wait dma int rst timeout\n");
			return -1;
		}
	}
	writel((1 << 0), &priv->reg->dmac); /* idma reset */
	timeout = timer_get_us() + 0xffff;
	while (readl(&priv->reg->dmac) & (1 << 0)) {
		if (timer_get_us() > timeout) {
			printf("wait dma rst timeout\n");
			return -1;
		}
	}
	writel((1 << 1) | (1 << 7), &priv->reg->dmac); /* idma on */
	rval = readl(&priv->reg->idie) & (~3);
	if (data->flags & MMC_DATA_WRITE)
		rval |= (1 << 0);
	else
		rval |= (1 << 1);
	writel(rval, &priv->reg->idie);
	writel(((size_t)pdes) >> 2, &priv->reg->dlba);
	writel((3U << 28) | (15 << 16) | 240,
	       &priv->reg->ftrglevel); /* burst-16, rx/tx trigger level=15/240 */

	return 0;
}

static int sunxi_mmc_send_cmd_common(struct sunxi_mmc_priv *priv,
				     struct mmc *mmc, struct mmc_cmd *cmd,
				     struct mmc_data *data)
{
	unsigned int cmdval = SUNXI_MMC_CMD_START;
	struct sunxi_mmc_des *pdes;
	unsigned int bytecnt = 0;
	unsigned int status = 0;
	u32 timeout = 0;
	int error = 0;

	if (priv->fatal_err)
		return -1;
	if (cmd->resp_type & MMC_RSP_BUSY)
		debug("mmc cmd %d check rsp busy\n", cmd->cmdidx);
	if (cmd->cmdidx == 12)
		return 0;

	pdes = NULL;

	if (!cmd->cmdidx)
		cmdval |= SUNXI_MMC_CMD_SEND_INIT_SEQ;
	if (cmd->resp_type & MMC_RSP_PRESENT)
		cmdval |= SUNXI_MMC_CMD_RESP_EXPIRE;
	if (cmd->resp_type & MMC_RSP_136)
		cmdval |= SUNXI_MMC_CMD_LONG_RESPONSE;
	if (cmd->resp_type & MMC_RSP_CRC)
		cmdval |= SUNXI_MMC_CMD_CHK_RESPONSE_CRC;

	if (data) {
		if ((u32)(long)data->dest & 0x3) {
			error = -1;
			goto out;
		}

		cmdval |= SUNXI_MMC_CMD_DATA_EXPIRE|SUNXI_MMC_CMD_WAIT_PRE_OVER;
		if (data->flags & MMC_DATA_WRITE)
			cmdval |= SUNXI_MMC_CMD_WRITE;
		if (data->blocks > 1)
			cmdval |= SUNXI_MMC_CMD_AUTO_STOP;
		writel(data->blocksize, &priv->reg->blksz);
		writel(data->blocks * data->blocksize, &priv->reg->bytecnt);
	} else {
		if ((cmd->cmdidx == 12)) {
			cmdval |= 1<<14;//stop current data transferin progress.
			cmdval &= ~(1 << 13);//Send command at once, even if previous data transfer has notcompleted
		}
	}

	debug("mmc %d, cmd %d(0x%08x), arg 0x%08x\n", priv->mmc_no,
	      cmd->cmdidx, cmdval | cmd->cmdidx, cmd->cmdarg);
	writel(cmd->cmdarg, &priv->reg->arg);

	if (!data)
		writel(cmdval | cmd->cmdidx, &priv->reg->cmd);

	if (data) {
		int ret = 0, buff_frag_num;
		bytecnt = data->blocksize * data->blocks;
		debug("mmc trans data %u bytes\n",
		       bytecnt);
		writel(readl(&priv->reg->gctrl) & (~0x80000000),
		       &priv->reg->gctrl);
		buff_frag_num = (bytecnt + SDXC_DES_BUFFER_MAX_LEN - 1) >> SDXC_DES_NUM_SHIFT;
		pdes = memalign(MMC_DMA_MINALIGN, sizeof(struct sunxi_mmc_des) * buff_frag_num);
		if (pdes == NULL) {
			printf("malloc pdes fail\n");
			goto out;
		}
		ret = mmc_trans_data_by_dma(priv, mmc, data, pdes);

		writel(cmdval | cmd->cmdidx, &priv->reg->cmd);
		if (ret) {
			error = readl(&priv->reg->rint) & 0xbbc2;
			if (!error)
				error = 0xffffffff;
			goto out1;
		}
	}
	timeout = timer_get_us() + 0xffffff;
	do {
		status = readl(&priv->reg->rint);
		if ((timer_get_us() > timeout) || (status & 0xbbc2)) {
			error = status & 0xbbc2;
			if (!error)
				error = 0xffffffff; /*represet software timeout*/
			printf("mmc cmd %u timeout, err %x\n",
				 cmd->cmdidx, error);
			goto out1;
		}
	} while (!(status & 0x4));

	if (data) {
		unsigned done = 0;
		timeout       =  timer_get_us() + (0xffffff);

		do {
			status = readl(&priv->reg->rint);
			if ((timer_get_us() > timeout) || (status & 0xbbc2)) {
				error = status & 0xbbc2;
				if (!error)
					error = 0xffffffff; /*represet software timeout*/
				printf("mmc data timeout, err %x\n",
					error);
				goto out1;
			}
			if (data->blocks > 1)
				done = status & (1 << 14);
			else
				done = status & (1 << 3);
		} while (!done);

		if ((data->flags & MMC_DATA_READ)) {
			timeout = timer_get_us() + 0xffffff;
			done    = 0;
			status  = 0;
			debug("mmc cacl rd dma timeout %x\n",
			       timeout);
			do {
				status = readl(&priv->reg->idst);
				if ((timer_get_us() > timeout) || (status & 0x234)) {
					error = status & 0x1E34;
					if (!error)
						error = 0xffffffff; /*represet software timeout*/
					printf("mmc wait dma over err %x\n",
						error);
					goto out1;
				}
				done = status & (1 << 1);
				/*usdelay(1);*/
			} while (!done);
		}
	}

	if (cmd->resp_type & MMC_RSP_BUSY) {
		timeout = timer_get_us() + 0x4ffffff;
		do {
			status = readl(&priv->reg->status);
			if (timer_get_us() > timeout) {
				error = -1;
				printf("mmc busy timeout, status %x\n",
					status);
				goto out1;
			}
		} while (status & (1 << 9));
	}
	if (cmd->resp_type & MMC_RSP_136) {
		cmd->response[0] = readl(&priv->reg->resp3);
		cmd->response[1] = readl(&priv->reg->resp2);
		cmd->response[2] = readl(&priv->reg->resp1);
		cmd->response[3] = readl(&priv->reg->resp0);
		debug("resp 0x%x 0x%x 0x%x 0x%x\n",
		       cmd->response[3], cmd->response[2], cmd->response[1],
		       cmd->response[0]);
	} else {
		cmd->response[0] = readl(&priv->reg->resp0);
		debug("mmc resp 0x%x\n", cmd->response[0]);
	}
out1:
	if ((data) && (pdes != NULL)) {
		free(pdes);
	}
out:
	if (data) {
		/* IDMASTAREG
		 * IDST[0] : idma tx int
		 * IDST[1] : idma rx int
		 * IDST[2] : idma fatal bus error
		 * IDST[4] : idma descriptor invalid
		 * IDST[5] : idma error summary
		 * IDST[8] : idma normal interrupt sumary
		 * IDST[9] : idma abnormal interrupt sumary
		 */
		status = readl(&priv->reg->idst);
		writel(status, &priv->reg->idst);
		writel(0, &priv->reg->idie);
		writel(0, &priv->reg->dmac);
		writel(readl(&priv->reg->gctrl) & (~(1 << 5)),
		       &priv->reg->gctrl);
		if (data->flags & MMC_DATA_READ)
			OSAL_CacheRangeInvaild(data->dest, data->blocksize * data->blocks, 0);
	}
	if (error) {
		writel(0x7, &priv->reg->gctrl);
		timeout = timer_get_us() + 0xffff;
		while (readl(&priv->reg->gctrl) & 0x7) {
			if (timer_get_us() > timeout) {
				printf("wait ctl reset timeout2\n");
				return -1;
			}
		}
		debug("mmc cmd %u err %x\n", cmd->cmdidx,
			error);
	}
	writel(0xffffffff, &priv->reg->rint);
	writel(readl(&priv->reg->gctrl) | SUNXI_MMC_GCTRL_FIFO_RESET,
	       &priv->reg->gctrl);

	if (error)
		return -1;
	else
		return 0;
}

static int sunxi_mmc_set_ios(struct udevice *dev)
{
	struct sunxi_mmc_plat *plat = dev_get_plat(dev);
	struct sunxi_mmc_priv *priv = dev_get_priv(dev);

	return sunxi_mmc_set_ios_common(priv, &plat->mmc);
}

static int sunxi_mmc_send_cmd(struct udevice *dev, struct mmc_cmd *cmd,
			      struct mmc_data *data)
{
	struct sunxi_mmc_plat *plat = dev_get_plat(dev);
	struct sunxi_mmc_priv *priv = dev_get_priv(dev);

	return sunxi_mmc_send_cmd_common(priv, &plat->mmc, cmd, data);
}

static const struct dm_mmc_ops sunxi_mmc_ops = {
	.send_cmd	= sunxi_mmc_send_cmd,
	.set_ios	= sunxi_mmc_set_ios,
};

static unsigned get_mclk_offset(void)
{
	return 0x830;
};

#define SUNXI_MMC0_BASE			0x04020000
#define SUNXI_CCU_BASE			0x02001000
static int sunxi_mmc_probe(struct udevice *dev)
{
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	struct sunxi_mmc_plat *plat = dev_get_plat(dev);
	struct sunxi_mmc_priv *priv = dev_get_priv(dev);
	struct reset_ctl_bulk reset_bulk;
	struct mmc_config *cfg = &plat->cfg;
	u32 *ccu_reg = (u32 *)SUNXI_CCU_BASE;
	int ret;

	cfg->name = dev->name;

	cfg->voltages = MMC_VDD_32_33 | MMC_VDD_33_34;
	cfg->host_caps = MMC_MODE_HS_52MHz | MMC_MODE_HS;
	cfg->host_caps |= MMC_MODE_4BIT;
	cfg->b_max = CONFIG_SYS_MMC_MAX_BLK_COUNT;

	cfg->f_min = 400000;
	cfg->f_max = 52000000;

	ret = mmc_of_parse(dev, cfg);
	if (ret)
		return ret;

	priv->reg = dev_read_addr_ptr(dev);

	/* Reset controller */
	writel(SUNXI_MMC_GCTRL_RESET, &priv->reg->gctrl);
	udelay(1000);

	/* bootloader have already init gpio and clk, so we can need not init that */
	priv->mmc_no = ((uintptr_t)priv->reg - SUNXI_MMC0_BASE) / 0x1000;
	priv->mclkreg = (void *)ccu_reg + get_mclk_offset() + priv->mmc_no * 4;

#if 0
	ret = clk_get_by_name(dev, "ahb", &gate_clk);
	if (!ret)
		clk_enable(&gate_clk);
#endif
	ret = reset_get_bulk(dev, &reset_bulk);
	if (!ret)
		reset_deassert_bulk(&reset_bulk);

	ret = mmc_set_mod_clk(priv, 24000000);
	if (ret)
		return ret;

	upriv->mmc = &plat->mmc;

	return 0;
}

static int sunxi_mmc_bind(struct udevice *dev)
{
	struct sunxi_mmc_plat *plat = dev_get_plat(dev);

	return mmc_bind(dev, &plat->mmc, &plat->cfg);
}

static const struct udevice_id sunxi_mmc_ids[] = {
	{ .compatible = "allwinner,sun20i-d1-mmc" },
};

U_BOOT_DRIVER(sunxi_mmc_drv) = {
	.name		= "sunxi_mmc",
	.id		= UCLASS_MMC,
	.of_match	= sunxi_mmc_ids,
	.bind		= sunxi_mmc_bind,
	.probe		= sunxi_mmc_probe,
	.ops		= &sunxi_mmc_ops,
	.plat_auto	= sizeof(struct sunxi_mmc_plat),
	.priv_auto	= sizeof(struct sunxi_mmc_priv),
};
