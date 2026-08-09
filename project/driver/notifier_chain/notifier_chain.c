// SPDX-License-Identifier: GPL-2.0
/*
 * notifier chain test
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/ktime.h>

static BLOCKING_NOTIFIER_HEAD(test_notifier_head);

#define ACTION_A 1
#define ACTION_B 2

/*
 * 内核通知链回调函数
 */
static int test_nb1_notifier_call(struct notifier_block *self,
				  unsigned long action, void *data)
{
	if (action == ACTION_A) {
		pr_info("%s :action = ACTION_A, data = %d\n", __func__,
			*(unsigned int *)data);
	} else if (action == ACTION_B) {
		pr_info("%s :action = ACTION_B, data = %d\n", __func__,
			*(unsigned int *)data);
	} else
		pr_info("%s :unkown action\n", __func__);

	return NOTIFY_DONE;
}

/*
 * 内核通知链结构体
 */
static struct notifier_block test_nb1 = {
	/*
	 * 通知链函数指针
	 */
	.notifier_call = test_nb1_notifier_call,
	/*
	 * 通知链优先级
	 */
	.priority = INT_MIN + 1,
};

static int test_nb2_notifier_call(struct notifier_block *self,
				  unsigned long action, void *data)
{
	if (action == ACTION_A) {
		pr_info("%s :action = ACTION_A, data = %d\n", __func__,
			*(unsigned int *)data);
	} else if (action == ACTION_B) {
		pr_info("%s :action = ACTION_B, data = %d\n", __func__,
			*(unsigned int *)data);
	} else
		pr_info("%s :unkown action\n", __func__);

	return NOTIFY_DONE;
}

static struct notifier_block test_nb2 = {
	.notifier_call = test_nb2_notifier_call,
	.priority = INT_MIN + 2,
};

static int __init notifier_chain_test_init(void)
{
	unsigned long action;
	unsigned int data;

	/*
	 * 注册内核通知链
	 */
	pr_info("notifier chain register\n");
	blocking_notifier_chain_register(&test_notifier_head, &test_nb1);
	blocking_notifier_chain_register(&test_notifier_head, &test_nb2);

	/*
	 * 调用内核通知链
	 */
	pr_info("notifier chain call acton = ACTION_A\n");
	action = ACTION_A;
	data = 10;
	blocking_notifier_call_chain(&test_notifier_head, action, &data);

	pr_info("notifier chain call acton = ACTION_B\n");
	action = ACTION_B;
	data = 1;
	blocking_notifier_call_chain(&test_notifier_head, action, &data);

	return 0;
}

static void __exit notifier_chain_test_exit(void)
{
	/*
	 * 注销内核通知链
	 */
	blocking_notifier_chain_unregister(&test_notifier_head, &test_nb1);
	blocking_notifier_chain_unregister(&test_notifier_head, &test_nb2);
}

MODULE_AUTHOR("Wangzai <1587636487@qq.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("This is notifier chain test");

module_init(notifier_chain_test_init);
module_exit(notifier_chain_test_exit);

