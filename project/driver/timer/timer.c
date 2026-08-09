// SPDX-License-Identifier: GPL-2.0
/*
 * timer test
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
#include <linux/timer.h>
#include <linux/interrupt.h>

static struct timer_list timer;

/*
 * 定时器回调函数
 */
void timer_callback(struct timer_list *timer)
{
	pr_info("timer expired!\n");
	/*
	 * 重新启动定时器
	 */
	mod_timer(timer, jiffies + msecs_to_jiffies(1000));
}

static int __init timer_test_init(void)
{
	/*
	 * 初始化定时器
	 */
	timer_setup(&timer, timer_callback, 0);

	/*
	 * 启动定时器，1秒后到期
	 */
	mod_timer(&timer, jiffies + msecs_to_jiffies(1000));

	return 0;
}

static void __exit timer_test_exit(void)
{
	del_timer(&timer);
}

MODULE_AUTHOR("Wangzai <1587636487@qq.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("This is timer test");

module_init(timer_test_init);
module_exit(timer_test_exit);

