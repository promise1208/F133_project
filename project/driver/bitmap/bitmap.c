// SPDX-License-Identifier: GPL-2.0
/*
 * bitmap test
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

/*
 * 定义bitmap
 */
#define BITMAP_LEN 128
static DECLARE_BITMAP(test_bitmap1, BITMAP_LEN);
static DECLARE_BITMAP(test_bitmap2, BITMAP_LEN);

static int __init bitmap_test_init(void)
{
	int i, len, start, align_mask;

	for (i = 0; i < 10; i++) {
		/*
		 * 设置特定的bit
		 */
		bitmap_set(test_bitmap1, i, 1);
	}

	/*
	 * 检查bitmap是否为全1
	 */
	if (bitmap_full(test_bitmap1, BITMAP_LEN))
		pr_info("bitmap full\n");
	else
		pr_info("bitmap not full\n");

	/*
	 * 将所有bit设为0
	 */
	bitmap_zero(test_bitmap1, BITMAP_LEN);

	/*
	 * 将所有bit设为1
	 */
	bitmap_fill(test_bitmap2, BITMAP_LEN);

	/*
	 * 将test_bitmap1与test_bitmap2的每个bit进行与操作
	 */
	bitmap_and(test_bitmap1, test_bitmap1, test_bitmap2, BITMAP_LEN);

	/*
	 * 检查test_bitmap1的所有bit是否为0
	 */
	if (bitmap_empty(test_bitmap1, BITMAP_LEN))
		pr_info("bitmap empty\n");
	else
		pr_info("bitmap not empty\n");

	/*
	 * 将所有bit设为0
	 */
	bitmap_zero(test_bitmap1, BITMAP_LEN);

	/*
	 * 将所有bit设为1
	 */
	bitmap_fill(test_bitmap2, BITMAP_LEN);

	/*
	 * 将test_bitmap1与test_bitmap2的每个bit进行或操作
	 */
	bitmap_or(test_bitmap1, test_bitmap1, test_bitmap2, BITMAP_LEN);

	/*
	 * 检查test_bitmap1的所有bit是否为0
	 */
	if (bitmap_full(test_bitmap1, BITMAP_LEN))
		pr_info("bitmap full\n");
	else
		pr_info("bitmap not full\n");

	/*
	 * 将所有bit设为0
	 */
	bitmap_zero(test_bitmap1, BITMAP_LEN);

	for (i = 0; i < 10; i++) {
		/*
		 * 设置特定的bit
		 */
		bitmap_set(test_bitmap1, i, 1);
	}

	/*
	 * bitmap_find_next_zero_area - find a contiguous aligned zero area
	 * @map: The address to base the search on
	 * @size: The bitmap size in bits
	 * @start: The bitnumber to start searching at
	 * @nr: The number of zeroed bits we're looking for
	 * @align_mask: Alignment mask for zero area
	 *
	 * The @align_mask should be one less than a power of 2; the effect is that
	 * the bit offset of all zero areas this function finds is multiples of that
	 * power of 2. A @align_mask of 0 means no alignment is required.
	 */
	/*
	 * 从start的bit处查找不需要对齐的连续2个为0的区域标签
	 */
	len = 2;
	start = 0;
	align_mask = 0;
	i = bitmap_find_next_zero_area(test_bitmap1, BITMAP_LEN,
				       start, len, align_mask);
	pr_info("idx = %d\n", i);

	return 0;
}

static void __exit bitmap_test_exit(void)
{
}

MODULE_AUTHOR("Wangzai <1587636487@qq.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("This is bitmap test");

module_init(bitmap_test_init);
module_exit(bitmap_test_exit);
