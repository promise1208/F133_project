// SPDX-License-Identifier: GPL-2.0
/*
 * platform driver test
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

struct test_platform_driver {
	void __iomem            *base;
};

/*
 * 驱动probe函数
 */
static int test_platform_driver_probe(struct platform_device *pdev)
{
	struct test_platform_driver *test_platform_driver;
	struct device *dev = &pdev->dev;
	struct resource *iores;
	int ret = 0;

	/*
	 * 申请驱动结构体内存
	 */
	test_platform_driver = devm_kzalloc(dev, sizeof(struct test_platform_driver), GFP_KERNEL);
	if (!test_platform_driver)
		return -ENOMEM;

	/*
	 * 获取platform device的相关参数
	 */
	iores = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!iores) {
		dev_err(dev, "Cannot find registers resource\n");
		return -ENOENT;
	}

	/*
	 * 将platform device传入的io地址转换为linux虚拟io地址。
	 */
	test_platform_driver->base = devm_ioremap_resource(dev, iores);
	if (IS_ERR(test_platform_driver->base)) {
		dev_err(dev, "Unable to ioremap registers\n");
		return PTR_ERR(test_platform_driver->base);
	}

	dev_info(dev, "test io base = 0x%lx\n", (unsigned long)test_platform_driver->base);

	return ret;
}

static int test_platform_driver_remove(struct platform_device *pdev)
{
	return 0;
}

/*
 * platform driver结构体
 * 改结构体包含了driver的probe函数和remove函数。
 */
static struct platform_driver test_platform_driver = {
	.probe  = test_platform_driver_probe,
	.remove = test_platform_driver_remove,
		.driver = {
		/*
		 * 驱动名。需要与设备结构体platform_deice中的name保持一致否则无法加载。
		 */
		.name   = "test_platform",
	},
};

static int __init platform_driver_test_init(void)
{
	/*
	 * 注册platform驱动
	 */
	return platform_driver_register(&test_platform_driver);
}

static void __exit platform_driver_test_exit(void)
{
	/*
	 * 注销platform驱动
	 */
	platform_driver_unregister(&test_platform_driver);
}

MODULE_AUTHOR("Wangzai <1587636487@qq.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("This is platform driver test");

module_init(platform_driver_test_init);
module_exit(platform_driver_test_exit);
