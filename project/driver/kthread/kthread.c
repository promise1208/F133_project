// SPDX-License-Identifier: GPL-2.0
/*
 * kthread test
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
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>

int val = 1;

/*
 * 线程处理函数
 */
static int kernel_thread(void *arg)
{
	int *val = (int *)arg;

	while (!kthread_should_stop()) {
		msleep(1000);
		pr_info("test_threads_handler: arg = %d\n", *val);
	}

	return 0;
}

static int __init kthread_test_init(void)
{
	struct task_struct *thread;

	/*
	 * 创建内核线程
	 */
	thread = kthread_create(kernel_thread, &val, "test_thread");
	if (IS_ERR(thread)) {
		pr_info("Failed to create waiting thread.\n");
		return PTR_ERR(thread);
	}

	/*
	 * 唤醒内核线程
	 */
	wake_up_process(thread);

	msleep(5000);
	/*
	 * 停止内核线程
	 */
	kthread_stop(thread);

	return 0;
}

static void __exit kthread_test_exit(void)
{
}

MODULE_AUTHOR("Wangzai <1587636487@qq.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("This is create kthread test");

module_init(kthread_test_init);
module_exit(kthread_test_exit);
