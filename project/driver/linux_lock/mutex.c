// SPDX-License-Identifier: GPL-2.0
/*
 * mutex test
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
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/delay.h>

static struct task_struct *thread1, *thread2;
static DEFINE_MUTEX(mutexlock);
static int shared_resource;

/*
 * 线程处理函数，thread1和thread2共用同一个线程处理函数。
 */
static int thread_handler(void *arg)
{
	pr_info("thread running\n");

	while (!kthread_should_stop()) {

		/*
		 * 互斥锁，如果没抢到锁，就会被调度走。
		 */
		mutex_lock(&mutexlock);

		shared_resource = 0;
		if (shared_resource == 1)
			pr_info("shared_resource verfiy to 1\n");

		shared_resource = 1;
		if (shared_resource == 0)
			pr_info("shared_resource verfiy to 0\n");
		mutex_unlock(&mutexlock);
	}

	return 0;
}

static int __init mutex_test_init(void)
{
	/*
	 * 申请两个线程，实现两个线程共同操作同一个变量。
	 */
	thread1 = kthread_run(thread_handler, (void *)1, "thread1");
	if (IS_ERR(thread1)) {
		pr_info("thread1 create fail\n");
		return -1;
	}
	thread2 = kthread_run(thread_handler, (void *)2, "thread2");
	if (IS_ERR(thread2)) {
		kthread_stop(thread1);
		pr_info("thread2 create fail\n");
		return -1;
	}

	return 0;
}

static void __exit mutex_test_exit(void)
{
	kthread_stop(thread1);
	kthread_stop(thread2);
}

MODULE_AUTHOR("Wangzai <1587636487@qq.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("This is mutex test");

module_init(mutex_test_init);
module_exit(mutex_test_exit);
