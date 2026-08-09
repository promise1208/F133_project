// SPDX-License-Identifier: GPL-2.0
/*
 * workqueue test
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
#include <linux/workqueue.h>

struct work_struct work1;
struct work_struct work2;
static struct workqueue_struct *work_queue;

static void work_func1(struct work_struct *work)
{
	pr_info("my_work_func 1 exec\n");
}

static void work_func2(struct work_struct *work)
{
	pr_info("my_work_func 2 exec\n");
}

static int __init workqueue_test_init(void)
{

	/*
	 * 创建一个工作队列
	 */
	work_queue = create_workqueue("dawn-workqueue");

	/*
	 * 初始化work
	 */
	INIT_WORK(&work1, work_func1);
	INIT_WORK(&work2, work_func2);


	/*
	 * 将work调度运行
	 */
	queue_work(work_queue, &work1);
	queue_work(work_queue, &work2);

	return 0;
}

static void __exit workqueue_test_exit(void)
{
	destroy_workqueue(work_queue);
}

MODULE_AUTHOR("Wangzai <1587636487@qq.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("This is workqueue test");

module_init(workqueue_test_init);
module_exit(workqueue_test_exit);
