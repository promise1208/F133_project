// SPDX-License-Identifier: GPL-2.0
/*
 * My first linux kernel code.
 *
 * Copyright (C) 2025 Wangzai 《搞linux的旺仔》
 *
 * This module serves no practical purpose and is solely for testing purposes.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>

static int __init fcode_init(void)
{
	int ret = 0;

	/*
	 * 打印当前函数
	 */
	pr_info("===> %s running\n", __func__);

	return ret;
}

static void __exit fcode_exit(void)
{
	pr_info("===> %s running\n", __func__);
}

MODULE_AUTHOR("Wangzai <1587636487@qq.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("This is my first code");

module_init(fcode_init);
module_exit(fcode_exit);
