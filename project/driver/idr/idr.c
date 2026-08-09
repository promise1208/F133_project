// SPDX-License-Identifier: GPL-2.0
/*
 *  Idr test code.
 *
 *  Copyright (C) 2025 Wangzai 《搞linux的旺仔》
 *
 *  This module for test idr.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/idr.h>

static int __init idr_test_init(void)
{
	char *s1 = "parm1", *s2 = "parm2", *s3 = "parm3", *st = "parm_tmp";
	int id1, id2, id3, idt;
	struct idr idr;
	char *ret_s;

	/*
	 * Init idr
	 */
	idr_init(&idr);

	/*
	 * Add element to idr
	 */
	id1 = idr_alloc(&idr, s1, 0, 10, GFP_KERNEL);
	id2 = idr_alloc(&idr, s2, 0, 10, GFP_KERNEL);
	id3 = idr_alloc(&idr, s3, 0, 10, GFP_KERNEL);

	/*
	 * Find idr element
	 */
	ret_s = idr_find(&idr, id1);
	pr_info("id1 [%d] = %s\n", id1, ret_s);

	ret_s = idr_find(&idr, id2);
	pr_info("id2 [%d] = %s\n", id2, ret_s);

	ret_s = idr_find(&idr, id3);
	pr_info("id3 [%d] = %s\n", id3, ret_s);

	/*
	 * Delete idr element.
	 */
	idr_remove(&idr, id2);

	/*
	 * Replace the idr parameter.
	 */
	idr_replace(&idr, st, id1);

	/*
	 * Check if the idr is empty.
	 */
	if (idr_is_empty(&idr))
		pr_info("idr empty\n");
	else
		pr_info("idr not empty\n");

	/*
	 * Compile each parameter of the idr in a loop.
	 */
	idr_for_each_entry(&idr, ret_s, idt)
		pr_info("id [%d] = %s\n", idt, ret_s);

	/*
	 * Free idr
	 */
	idr_destroy(&idr);

	return 0;
}

static void __exit idr_test_exit(void)
{
}

MODULE_AUTHOR("Wangzai <1587636487@qq.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("This is idr test code");

module_init(idr_test_init);
module_exit(idr_test_exit);

