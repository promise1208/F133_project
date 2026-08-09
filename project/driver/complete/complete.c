// SPDX-License-Identifier: GPL-2.0
/*
 * complete test
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
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>

static struct completion completion;

/*
 * 线程处理函数
 */
static int complete_test_thread(void *arg)
{
	pr_info("kernel thread running\n");
	msleep(3000);
	pr_info("kernel threads complete\n");
	/*
	 * 利用complete()通知主线程完成
	 */
	complete(&completion);

	return 0;
}

static int __init complete_test_init(void)
{
	struct task_struct *thread;

	init_completion(&completion);
	/*
	 * 创建线程
	 */
	thread = kthread_run(complete_test_thread, NULL, "complete_test_thread");
	if (IS_ERR(thread)) {
		pr_info("Failed to create waiting thread.\n");
		return PTR_ERR(thread);
	}

	pr_info("Main thread: Waiting for completion\n");
	/*
	 * 利用complete机制等待其他线程完成
	 */
	wait_for_completion(&completion);

	if (completion_done(&completion))
		pr_info("Main thread: Detected completion done\n");

	return 0;
}

static void __exit complete_test_exit(void)
{
}

MODULE_AUTHOR("Wangzai <1587636487@qq.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("This is complete test");

module_init(complete_test_init);
module_exit(complete_test_exit);

