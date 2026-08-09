// SPDX-License-Identifier: GPL-2.0
/*
 * kfifo test
 *
 * Copyright (C) 2025 Wangzai 《搞linux的旺仔》
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/kfifo.h>

#define KFIFO_SIZE 256

static int __init kfifo_test_init(void)
{
	char buffer[8], val[2];
	struct kfifo kfifo;
	int ret = 0;

	/*
	 * 申请一个深度为KFIFO_SIZE的kfifo
	 */
	ret = kfifo_alloc(&kfifo, KFIFO_SIZE, GFP_KERNEL);
	if (ret) {
		pr_info("Kfifo mallo fail\n");
		return -EINVAL;
	}

	/*
	 * 往kfifo中压入元素
	 */
	buffer[0] = 1;
	buffer[1] = 2;
	kfifo_in(&kfifo, buffer, 2);
	buffer[0] = 3;
	buffer[1] = 4;
	kfifo_in(&kfifo, buffer, 2);

	pr_info("currtent kfifo len = %d\n", kfifo_len(&kfifo));
	/*
	 * 获取kfifo的元素
	 */
	if (kfifo_out(&kfifo, &val, 2) != 2) {
		ret = -EINVAL;
		goto fail;
	}

	pr_info("first get val = %d, %d\n", val[0], val[1]);
	/*
	 * 判断kfifo是否为空
	 */
	if (kfifo_is_empty(&kfifo))
		pr_info("current kfifio is empty\n");
	else
		/*
		 * 如果kfifo不为空，则获取kfifo的长度
		 */
		pr_info("current kfifo len = %d\n", kfifo_len(&kfifo));

	if (kfifo_out(&kfifo, &val, 2) != 2) {
		ret = -EINVAL;
		goto fail;
	}

	pr_info("second get val = %d, %d\n", val[0], val[1]);
	if (kfifo_is_empty(&kfifo))
		pr_info("current kfifio is empty\n");
	else
		pr_info("current kfifo len = %d\n", kfifo_len(&kfifo));

fail:
	/*
	 * 释放kfifo
	 */
	kfifo_free(&kfifo);

	return 0;
}

static void __exit kfifo_test_exit(void)
{
}

MODULE_AUTHOR("Wangzai <1587636487@qq.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("This is kfifo test");

module_init(kfifo_test_init);
module_exit(kfifo_test_exit);
