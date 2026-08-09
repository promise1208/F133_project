// SPDX-License-Identifier: GPL-2.0
/*
 * sysfs test
 *
 * Copyright (C) 2025 Wangzai 《搞linux的旺仔》
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * 设备树添加如下描述:
 * sysfs_device:sysfs_device {
 *     compatible = "dawn,test_sysfs";
 * };
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>

static ssize_t sysfs_devop_test_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	int val = 10;

	return sprintf(buf, "%d\n", val);
}

static ssize_t sysfs_devop_test_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf,
				   size_t count)
{
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 10, &val);
	if (ret)
		return ret;

	pr_info("store val = %ld\n", val);

	return count;
}
static DEVICE_ATTR_RW(sysfs_devop_test);

static struct attribute *sysfs_devop_attributes[] = {
	&dev_attr_sysfs_devop_test.attr,
	NULL
};

static const struct attribute_group sysfs_devop_attr_group = {
	.attrs = sysfs_devop_attributes,
};

/*
 * 驱动probe函数
 */
static int test_sysfs_probe(struct platform_device *pdev)
{
	return sysfs_create_group(&pdev->dev.kobj, &sysfs_devop_attr_group);
}

static int test_sysfs_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &sysfs_devop_attr_group);

	return 0;
}

static const struct of_device_id test_sysfs_match[] = {
	{ .compatible = "dawn,test_sysfs" },
	{},
};
MODULE_DEVICE_TABLE(of, test_sysfs_match);

/*
 * test_sysfs结构体
 * 改结构体包含了driver的probe函数和remove函数。
 */
static struct platform_driver test_sysfs = {
	.probe  = test_sysfs_probe,
	.remove = test_sysfs_remove,
		.driver = {
			.of_match_table = test_sysfs_match,
			.name = "test_sysfs"
		},
};

static int __init sysfs_test_init(void)
{
	/*
	 * 注册platform驱动
	 */
	return platform_driver_register(&test_sysfs);
}

static void __exit sysfs_test_exit(void)
{
	/*
	 * 注销platform驱动
	 */
	platform_driver_unregister(&test_sysfs);
}

MODULE_AUTHOR("Wangzai <1587636487@qq.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("This is sysfs test");

module_init(sysfs_test_init);
module_exit(sysfs_test_exit);
