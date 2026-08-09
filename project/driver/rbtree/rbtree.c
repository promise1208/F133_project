// SPDX-License-Identifier: GPL-2.0
/*
 * rbtree test
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
 * 定义一个红黑树结构体
 */
struct par {
	u32		id;
	char		*name;
	struct rb_node	node;
};

/*
 * 通过id查找对应的结构体
 */
static struct par *rbtree_find_by_id(struct rb_root *root, u32 id)
{
	struct rb_node *node;
	struct par *par;
	
	node = root->rb_node;
	while (node) {
		par = rb_entry(node, struct par, node);
		if (par->id < id)
			node = node->rb_right;
		else if (par->id > id)
			node = node->rb_left;
		else
			return par;
	}
	
	return NULL;
}

/*
 * 往红黑树中插入元素
 */
static void rbtree_insert(struct rb_root *root, struct par *par)
{
	struct rb_node **new_node, *parent_node = NULL;
	struct par *curr_par;

	new_node = &(root->rb_node);
	while (*new_node) {
		parent_node = *new_node;
		curr_par = rb_entry(parent_node, struct par,
		                      node);
		if (curr_par->id > par->id) {
			new_node = &((*new_node)->rb_left);
		} else if (curr_par->id < par->id) {
			new_node = &((*new_node)->rb_right);
		} else {
			printk("%u already in tree\n", par->id);
			return;
		}
	}
	
	rb_link_node(&par->node, parent_node, new_node);
	rb_insert_color(&par->node, root);
}

/*
 * 删除红黑树中对应的元素
 */
static void rbtree_delete(struct rb_root *root, struct par *par)
{
	rb_erase(&par->node, root);
}

/*
 * 删除红黑树中所有元素
 */
static void rbtree_delete_all(struct rb_root *root)
{
	struct rb_node *node, *next;
	struct par *curr_par;

	for (node = rb_first(root);
		next = node ? rb_next(node) : NULL, node != NULL;
			node = next) {
		curr_par = rb_entry(node, struct par, node);
		rbtree_delete(root, curr_par);
	}
}

static int __init rbtree_init(void)
{
	struct rb_root root = {};

	struct par par1 = {
		.id = 1,
		.name = "one",
	};
	struct par par2 = {
		.id = 2,
		.name = "two",
	};
	struct par par3 = {
		.id = 3,
		.name = "three",
	};

	rbtree_insert(&root, &par1);
	rbtree_insert(&root, &par2);
	rbtree_insert(&root, &par3);

	printk("find name = %s\n", rbtree_find_by_id(&root, 1)->name);
	printk("find name = %s\n", rbtree_find_by_id(&root, 2)->name);
	printk("find name = %s\n", rbtree_find_by_id(&root, 3)->name);

	rbtree_delete_all(&root);

	return 0;
}

static void __exit rbtree_exit(void)
{
}

MODULE_AUTHOR("Wangzai <1587636487@qq.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("This is rbtree test");

module_init(rbtree_init);
module_exit(rbtree_exit);
