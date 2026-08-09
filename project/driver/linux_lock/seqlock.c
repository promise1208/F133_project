// SPDX-License-Identifier: GPL-2.0
/*
 * seqlock test
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
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/seqlock.h>
#include <linux/delay.h>

static seqlock_t seqlock;
struct task_struct *threads[4];
static int shared_data;

/*
 * 读线程函数。
 */
static int read_handler(void *arg)
{
	unsigned seq;
	int val;

	pr_info("read thread running\n");

	while (!kthread_should_stop()) {

		do {
			seq = read_seqbegin(&seqlock);
			val = shared_data;
		} while (read_seqretry(&seqlock, seq));
	
		pr_info("read shared_data = %d\n", shared_data);
	}

	return 0;
}

/*
 * 写线程函数。
 */
static int write_handler(void *arg)
{
	pr_info("write thread running\n");

	while (!kthread_should_stop()) {

		/*
		 * 获取写锁。
		 */
		write_seqlock(&seqlock);
	
		shared_data += 1;
		pr_info("write shared_data = %d\n", shared_data);
	
		/*
		 * 释放写锁。
		 */
		write_sequnlock(&seqlock);
	}

	return 0;
}

static int __init seqlock_test_init(void)
{
	int i, j;

	/*
	 * 初始化seqlock。
	 */
	seqlock_init(&seqlock);

	/*
	 * 创建2个写线程。
	 */
	for (i = 0; i < 2; i++) {
		threads[i] = kthread_run(write_handler, NULL, "write_thread");
		if (IS_ERR(threads[i]))
			goto ERR;
	}
	/*
	 * 创建2个读者线程。
	 */
	for (i = 2; i < 4; i++) {
		threads[i] = kthread_run(read_handler, NULL, "read_thread");
		if (IS_ERR(threads[i]))
			goto ERR;
	}

	return 0;
ERR:
	/*
	 * 处理申请线程失败。
	 */
	for (j = 0; j < i; j++)
		kthread_stop(threads[j]);

	return -1;
}

static void __exit seqlock_test_exit(void)
{
	for (int i = 0; i < 4; i++)
		kthread_stop(threads[i]);
}

MODULE_AUTHOR("Wangzai <1587636487@qq.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("This is seqlock test");

module_init(seqlock_test_init);
module_exit(seqlock_test_exit);

