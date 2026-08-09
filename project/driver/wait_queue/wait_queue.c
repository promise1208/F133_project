// SPDX-License-Identifier: GPL-2.0
/*
 * waitqueue test
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

wait_queue_head_t wq;

/*
 * 线程处理函数
 */
static int waitqueue_test_thread(void *arg)
{
	/*
	 * 初始化wait
	 */
	DEFINE_WAIT(wait);
	/*
	 * 等待前准备
	 */
	prepare_to_wait(&wq, &wait, TASK_INTERRUPTIBLE);

	pr_info("kernel thread start wait\n");

	/*
	 * 利用schedule()将任务调度走
	 */
	schedule();

	finish_wait(&wq, &wait);
	pr_info("kernel thread wake up\n");

	return 0;
}

static int __init waitqueue_test_init(void)
{
	struct task_struct *thread;

	/*
	 * 初始化waitqueue
	 */
	init_waitqueue_head(&wq);

	/*
	 * 创建内核线程
	 */
	thread = kthread_run(waitqueue_test_thread, NULL, "waitqueue_test_thread");
	if (IS_ERR(thread)) {
		pr_info("Failed to create waiting thread.\n");
		return PTR_ERR(thread);
	}

	msleep(3000);

	pr_info("main process wake up kernel thread\n");

	/*
	 * 唤醒等待的waitqueue
	 */
	wake_up_interruptible(&wq);

	/*
	 * 停止内核线程
	 */
	kthread_stop(thread);

	return 0;
}

static void __exit waitqueue_test_exit(void)
{
}

MODULE_AUTHOR("Wangzai <1587636487@qq.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("This is waitqueue test");

module_init(waitqueue_test_init);
module_exit(waitqueue_test_exit);
