// SPDX-License-Identifier: GPL-2.0
/*
 * atomic test
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
#include <linux/atomic.h>
#include <linux/kthread.h>
#include <linux/delay.h>

static atomic_t counter = ATOMIC_INIT(0); // 初始化原子变量

static int __init atomic_test_init(void)
{
	/*
	 * 原子设置counter的值为10。
	 */
	atomic_set(&counter, 10);
	pr_info("counter val = %d\n", atomic_read(&counter));
	/*
	 * 原子将counter加1。
	 */
	atomic_inc(&counter);
	pr_info("counter val = %d\n", atomic_read(&counter));

	/*
	 * 原子将counter加5。
	 */
	atomic_add(5, &counter);
	pr_info("counter val = %d\n", atomic_read(&counter));

	/*
	 * 原子将counter减2。
	 */
	atomic_sub(2, &counter);
	pr_info("counter val = %d\n", atomic_read(&counter));

	return 0;
}

static void __exit atomic_test_exit(void)
{
}

MODULE_AUTHOR("Wangzai <1587636487@qq.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("This is atomic test");

module_init(atomic_test_init);
module_exit(atomic_test_exit);
