// SPDX-License-Identifier: GPL-2.0
/*
 * platform device test
 *
 * Copyright (C) 2025 Wangzai 《搞linux的旺仔》
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>

/*
 * 设备资源结构体，包含设备的一些参数。
 */
static struct resource test_platform_device_resources[] = {
	[0] = {
		.start	= 0x12000000,
		.end	= 0x12000000 + 0x1000000,
		.flags	= IORESOURCE_MEM,
	},
};

/*
 * platform device结构体
 * 改结构体包含了设备信息，驱动通过设备信息执行对于的初始化。
 */
static struct platform_device test_platform_device = {
	/*
	 * 设备名。要与驱动结构体platform_driver.driver.name保持一致，
	 * 否则无法加载。
	 */
	.name   = "test_platform",
	.resource = test_platform_device_resources,
	.num_resources = 1,
};

static int __init platform_device_test_init(void)
{
	/*
	 * 注册platform设备
	 */
	return platform_device_register(&test_platform_device);
}

static void __exit platform_device_test_exit(void)
{
	/*
	 * 注销platform设备
	 */
	platform_device_unregister(&test_platform_device);
}

MODULE_AUTHOR("Wangzai <1587636487@qq.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("This is platform device test");

module_init(platform_device_test_init);
module_exit(platform_device_test_exit);
