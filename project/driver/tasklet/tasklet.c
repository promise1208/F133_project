// SPDX-License-Identifier: GPL-2.0
/*
 * tasklet test
 *
 * Copyright (C) 2025 Wangzai 《搞linux的旺仔》
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include<linux/init.h>
#include<linux/fs.h>
#include <linux/kernel.h>
#include<linux/interrupt.h>

static struct tasklet_struct tasklet;

/*
 * tasklet处理函数
 */
static void tasklet_handler(unsigned long data)
{
	pr_info("%s running, data = %ld\n", __func__, data);
}

static int __init tasklet_test_init(void)
{
	unsigned long data = 12;

	/*
	 * 初始化tasklet
	 */
	tasklet_init(&tasklet, tasklet_handler, data);

	/*
	 * 使能tasklet
	 */
	tasklet_enable(&tasklet);

	/*
	 * 调度tasklet
	 */
	tasklet_schedule(&tasklet);

	/*
	 * 失能tasklet
	 */
	tasklet_disable(&tasklet);
	/*
	 * 调度tasklet
	 */
	tasklet_schedule(&tasklet);

	return 0;
}

static void __exit tasklet_test_exit(void)
{
	tasklet_kill(&tasklet);
}

MODULE_AUTHOR("Wangzai <1587636487@qq.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("This is tasklet test");

module_init(tasklet_test_init);
module_exit(tasklet_test_exit);

