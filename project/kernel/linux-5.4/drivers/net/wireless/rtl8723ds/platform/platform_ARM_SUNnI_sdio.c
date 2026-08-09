/******************************************************************************
 *
 * Copyright(c) 2013 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/
/*
 * Description:
 *	This file can be applied to following platforms:
 *	CONFIG_PLATFORM_ARM_SUN6I
 *	CONFIG_PLATFORM_ARM_SUN7I
 *	CONFIG_PLATFORM_ARM_SUN8I
 */
#include <drv_types.h>
//#include <mach/sys_config.h>
#ifdef CONFIG_GPIO_WAKEUP
#include <linux/gpio.h>
#endif

#ifdef CONFIG_MMC
static int sdc_id = -1;
static signed int gpio_eint_wlan = -1;
static u32 eint_wlan_handle = 0;

#if defined(CONFIG_PLATFORM_ARM_SUN6I) || defined(CONFIG_PLATFORM_ARM_SUN7I)
extern void sw_mci_rescan_card(unsigned id, unsigned insert);
#elif defined(CONFIG_PLATFORM_ARM_SUN8I)
extern void sunxi_mci_rescan_card(unsigned id, unsigned insert);
#endif

#ifdef CONFIG_PLATFORM_ARM_SUN8I_W5P1
extern int get_rf_mod_type(void);
#else
extern int wifi_pm_get_mod_type(void);
#endif

extern void wifi_pm_power(int on);
#ifdef CONFIG_GPIO_WAKEUP
extern unsigned int oob_irq;
#endif
#endif /* CONFIG_MMC */

/*
 * Return:
 *	0:	power on successfully
 *	others: power on failed
 */
int platform_wifi_power_on(void)
{
	int ret = 0;

	return ret;
}

void platform_wifi_power_off(void)
{
}
