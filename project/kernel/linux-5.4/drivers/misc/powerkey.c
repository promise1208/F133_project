/*
 * powerkey device driver
 *
 * Copyright (C) 2010 xxxx Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
 /*
  * 设备树描述:
  *     pkey:pkey {
  *         compatible = "wangzai,powerkey";
  *         poweroff-gpios = <&pio PG 14 GPIO_ACTIVE_HIGH>;
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

#define POWERKEY_DBG
#ifdef  POWERKEY_DBG
#define powerkey_dbg(fmt, ...) \
	printk(fmt, ##__VA_ARGS__)
#else
#define powerkey_dbg(fmt, ...) \
        ;
#endif

#define DRIVER_NAME "powerkey"

struct powerkey_stu {
	char *name;
	struct device *dev;
	struct gpio_desc *gpiod;
	int irq;
};

static irqreturn_t key_thread_work(int irq, void *dev)
{
	struct powerkey_stu *powerkey = dev;
	char *cmd = "/sbin/poweroff";
	static char *envp[] = {
		"HOME=/",
		"PATH=/sbin:/bin:/usr/sbin:/usr/bin",
		NULL
	};
	char **argv;

	dev_info(powerkey->dev, "key work\n");

	argv = argv_split(GFP_KERNEL, cmd, NULL);
	if (argv) {
		call_usermodehelper("/sbin/poweroff", argv, envp, UMH_WAIT_EXEC);
		argv_free(argv);
	} 
	
	return IRQ_HANDLED;
}

static irqreturn_t key_handler(int irq, void *dev)
{
	struct powerkey_stu *powerkey = dev;

	dev_info(powerkey->dev, "key press\n");

	return IRQ_WAKE_THREAD;
}

static int powerkey_probe(struct platform_device *pdev)
{
	struct powerkey_stu *powerkey;
	int ret = 0;

	powerkey = devm_kmalloc(&pdev->dev, sizeof(struct powerkey_stu), GFP_KERNEL);
	if (!powerkey) {
		dev_err(&pdev->dev, "Failed to allocate driver data\n");
		return -ENOMEM;
	}

	powerkey->gpiod = devm_gpiod_get(&pdev->dev, "poweroff", GPIOD_IN);
	if (IS_ERR(powerkey->gpiod)) {
		dev_err(&pdev->dev, "Failed to get gpiod\n");
		return PTR_ERR(powerkey->gpiod);
	}

	powerkey->irq = gpiod_to_irq(powerkey->gpiod);
	if (powerkey->irq <= 0)
		return powerkey->irq < 0 ? powerkey->irq : -EINVAL;

	ret = devm_request_threaded_irq(&pdev->dev, powerkey->irq,
					key_handler,
					key_thread_work,
					IRQF_TRIGGER_FALLING,
					"poweroff key driver",
					powerkey);
	if (ret) {
		dev_err(&pdev->dev, "Unable to acquire irq!\n");
		return ret;
	}

	powerkey->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, powerkey);
	dev_info(&pdev->dev, "Probe success\n");

	return 0;
}

static const struct of_device_id powerkey_match[] = {
	{ .compatible = "wangzai,powerkey" },
	{},
};
MODULE_DEVICE_TABLE(of, powerkey_match);

static struct platform_driver powerkey_platform_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = powerkey_match,
	},
	.probe = powerkey_probe,
};

static int __init powerkey_init(void)
{
	return platform_driver_register(&powerkey_platform_driver);
}

static void __exit powerkey_exit(void)
{
	platform_driver_unregister(&powerkey_platform_driver);
}

module_init(powerkey_init);
module_exit(powerkey_exit);

MODULE_AUTHOR("wangzai");
MODULE_DESCRIPTION("powerkey driver");
MODULE_LICENSE("GPL");
