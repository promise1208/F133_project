// SPDX-License-Identifier: GPL-2.0
/*
 * key interrupt test
 *
 * Copyright (C) 2025 Wangzai 《搞linux的旺仔》
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
 /*
  * 设备树描述:
  *     pkey:pkey {
  *         compatible = "dawn,dawn_key";
  *         poweroff-gpios = <&pio PC 5 GPIO_ACTIVE_HIGH>;
  *     };
  */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/module.h>

#define DRIVER_NAME "dawn_key"

struct dawn_key_stu {
	char *name;
	struct device *dev;
	struct gpio_desc *gpiod;
	int irq;
};

/*
 * 按键中断线程化处理函数
 * 将耗时的操作放在这里。
 */
static irqreturn_t key_thread_work(int irq, void *dev)
{
	struct dawn_key_stu *dawn_key = dev;

	dev_info(dawn_key->dev, "key thread work\n");

	return IRQ_HANDLED;
}

/*
 * 按键中断处理函数
 * 中断处理函数要求快进快出。
 */
static irqreturn_t key_handler(int irq, void *dev)
{
	struct dawn_key_stu *dawn_key = dev;

	dev_info(dawn_key->dev, "key press\n");

	return IRQ_WAKE_THREAD;
}

/*
 * 按键中断probe函数
 */
static int dawn_key_probe(struct platform_device *pdev)
{
	struct dawn_key_stu *dawn_key;
	int ret = 0;

	dawn_key = devm_kmalloc(&pdev->dev, sizeof(struct dawn_key_stu), GFP_KERNEL);
	if (!dawn_key)
		return -ENOMEM;

	/*
	 * 通过设备树描述符获取gpio desc
	 */
	dawn_key->gpiod = devm_gpiod_get(&pdev->dev, "poweroff", GPIOD_IN);
	if (IS_ERR(dawn_key->gpiod)) {
		dev_err(&pdev->dev, "Failed to get gpiod\n");
		return PTR_ERR(dawn_key->gpiod);
	}

	/*
	 * 获取按键中断号
	 */
	dawn_key->irq = gpiod_to_irq(dawn_key->gpiod);
	if (dawn_key->irq <= 0)
		return dawn_key->irq < 0 ? dawn_key->irq : -EINVAL;

	/*
	 * 通过线程化中断申请函数devm_request_thread_irq()申请中断
	 */
	ret = devm_request_threaded_irq(&pdev->dev, dawn_key->irq,
					key_handler,
					key_thread_work,
					IRQF_TRIGGER_FALLING,
					"key interrupt",
					dawn_key);
	if (ret) {
		dev_err(&pdev->dev, "Unable to acquire irq!\n");
		return ret;
	}

	dawn_key->dev = &pdev->dev;
	/*
	 * 设置私有数据
	 */
	dev_set_drvdata(&pdev->dev, dawn_key);
	dev_info(&pdev->dev, "Probe success\n");

	return 0;
}

static const struct of_device_id dawn_key_match[] = {
	{ .compatible = "dawn,dawn_key" },
	{},
};
MODULE_DEVICE_TABLE(of, dawn_key_match);

static struct platform_driver dawn_key_platform_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = dawn_key_match,
	},
	.probe = dawn_key_probe,
};

static int __init dawn_key_init(void)
{
	return platform_driver_register(&dawn_key_platform_driver);
}

static void __exit dawn_key_exit(void)
{
	platform_driver_unregister(&dawn_key_platform_driver);
}

module_init(dawn_key_init);
module_exit(dawn_key_exit);

MODULE_AUTHOR("wangzai");
MODULE_DESCRIPTION("dawn_key driver");
MODULE_LICENSE("GPL");
