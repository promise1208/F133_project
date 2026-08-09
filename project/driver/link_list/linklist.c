// SPDX-License-Identifier: GPL-2.0
/*
 * link list test
 *
 * Copyright (C) 2025 Wangzai 《搞linux的旺仔》
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/slab.h>

/*
 * 定义一个链表结构体
 */
struct element {
	struct list_head list;
#define NAME_LEN 64
	char name[NAME_LEN];
};

/*
 *申请链表元素
 */
static struct element *alloc_ele(char *name)
{
	struct element *ele;

	ele = kmalloc(sizeof(struct element), GFP_KERNEL);
	if (!ele)
		return NULL;
	strcpy(ele->name, name);

	return ele;
}

static void free_ele(struct element *ele)
{
	kfree(ele);
}

static int __init link_list_init(void)
{
	struct element hele;
	struct element *ele, *tmp_ele;
	struct element *ele1, *ele2, *ele3;

	/*
	 * 初始化链表头
	 */
	INIT_LIST_HEAD(&hele.list);

	/*
	 * 申请链表
	 */
	ele1 = alloc_ele("p1");
	if (!ele1)
		return -ENOMEM;

	/*
	 * 将链表添加到链表链中
	 */
	list_add_tail(&ele1->list, &hele.list);

	ele2 = alloc_ele("p2");
	if (!ele2) {
		free_ele(ele1);
		return -ENOMEM;
	}
	list_add_tail(&ele2->list, &hele.list);

	ele3 = alloc_ele("p3");
	if (!ele3) {
		free_ele(ele2);
		free_ele(ele1);
		return -ENOMEM;
	}
	list_add_tail(&ele3->list, &hele.list);

	/*
	 * 遍历删除链表中特定元素
	 * 这里使用list_for_each_entry_safe()而非list_for_each_entry()原因是
	 * 由于在循环链表中要删除元素，而list_for_each_entry()不支持此种操作
	 */
	pr_info("start delete ele\n");
	list_for_each_entry_safe(ele, tmp_ele, &hele.list, list) {
		pr_info("ele->name = %s\n", ele->name);
		if (!strcmp(ele->name, "p3")) {
			list_del(&ele->list);
			free_ele(ele);
		}
	}

	pr_info("start get ele\n");
	list_for_each_entry_safe(ele, tmp_ele, &hele.list, list) {
		pr_info("ele->name = %s\n", ele->name);
		list_del(&ele->list);
		free_ele(ele);
	}

	if (list_empty(&hele.list))
		pr_info("link list is empty\n");
	else
		pr_info("link list not empty\n");

	return 0;
}

static void __exit link_list_exit(void)
{
}

MODULE_AUTHOR("Wangzai <1587636487@qq.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("This is link list test");

module_init(link_list_init);
module_exit(link_list_exit);
