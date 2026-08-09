// SPDX-License-Identifier: GPL-2.0
/*
 * dump stack test
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

static noinline void function_1(void)
{
	dump_stack();
}

static noinline void function_2(void)
{
	function_1();
}

static noinline void function_3(void)
{
	function_2();
}

static int __init dump_stack_test_init(void)
{

	function_3();

	return 0;
}

static void __exit dump_stack_test_exit(void)
{
}

MODULE_AUTHOR("Wangzai <1587636487@qq.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("This is dump stack test");

module_init(dump_stack_test_init);
module_exit(dump_stack_test_exit);
