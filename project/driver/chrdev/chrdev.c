// SPDX-License-Identifier: GPL-2.0
/*
 * chrdev test
 *
 * Copyright (C) 2025 Wangzai 《搞linux的旺仔》
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

#define CHARDEV_DEV_CNT (1)
#define CHARDEV_DEV_NAME "chrdev_dev"
#define CHARDEV_CLASS_NAME "chrdev_class"
#define CHARDEV_CDEV_NAME "chrdev_chrdev"
#define MALLOC_LEN (128)

#define CHRDEV_DBG pr_info("func = %s, line = %d\n", __func__, __LINE__)

struct chrdev_dev {
	dev_t         devid;
	struct cdev   *cdev;
	struct class  *class;
	struct device *dev;
};

static struct chrdev_dev chrdev;

/*
 * 字符设备open()函数
 */
static int chrdev_open(struct inode *inode_p, struct file *file_p)
{
	char *mem = NULL;

	CHRDEV_DBG;

	/*
	 * 申请一段内存并保存到文件的私有数据中。
	 */
	mem = kmalloc(MALLOC_LEN, GFP_KERNEL);
	if (!mem)
		return -1;
	file_p->private_data = mem;

	return 0;
}

/*
 * 字符设备release()函数，即关闭函数
 */
static int chrdev_release(struct inode *inode_p, struct file *file_p)
{
	char *mem = (char *)file_p->private_data;

	CHRDEV_DBG;

	/*
	 * 通过kfree()函数释放在chrdev_open()函数中申请的内存。
	 */
	kfree(mem);

	return 0;
}

/*
 * 字符设备读函数
 */
static ssize_t chrdev_read(struct file *file_p, char __user *buf, size_t size,
			   loff_t *ppos)
{
	char *mem = (char *)file_p->private_data;
	int rc;

	CHRDEV_DBG;

	/*
	 * 通过copy_to_user()函数将内核空间的数据(mem)拷贝到应用程序空间(buf)。
	 */
	rc = copy_to_user(buf, mem, size);
	if (rc)
		return -1;

	return size;
}

/*
 * 字符设备写函数
 */
static ssize_t chrdev_write(struct file *file_p, const char __user *buf,
			    size_t size, loff_t *ppos)
{
	char *mem = (char *)file_p->private_data;
	int rc;

	CHRDEV_DBG;

	/*
	 * 通过copy_from_user()函数将应用程序空间的数据(buf拷贝到内核空间(mem)。
	 */
	rc = copy_from_user(mem, buf, size);
	if (rc)
		return -1;

	return size;
}

/*
 * 字符设备iotctl()函数，通过cmd传递命令，通过arg传递数据
 */
static long chrdev_unlocked_ioctl(struct file *file_p, unsigned int cmd,
				  unsigned long arg)
{
	unsigned int val = 0;
	int rc;

	CHRDEV_DBG;
	rc = copy_from_user((void *)&val, (const char __user *)arg, sizeof(val));
	if (rc)
		return -1;
	pr_info("cmd = %d, val = %d\n", cmd, val);

	return 0;
}

/*
 * 文件操作结构体
 */
static const struct file_operations chrdev_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = chrdev_unlocked_ioctl,
	.read = chrdev_read,
	.write = chrdev_write,
	.open = chrdev_open,
	.release = chrdev_release,
};

static int __init chrdev_test_init(void)
{
	int major = 0, minor = 0;

	/*
	 * 注册一个字符设备。
	 * 本质是创建一组与设备号绑定的文件读写接口。
	 *
	 * 第一个参数为传入的major,当major = 0时，自动获取major。
	 */
	major = register_chrdev(0, CHARDEV_CDEV_NAME, &chrdev_fops);
	if (!major)
		return -1;
	chrdev.devid = MKDEV(major, minor);

	/*
	 * 创建一个class。
	 */
	chrdev.class = class_create(THIS_MODULE, CHARDEV_CLASS_NAME);
	if (IS_ERR(chrdev.class))
		goto UNREG_CHRDEV;

	/*
	 * 创建一个class下面的设备，并通过设备号与字符设备绑定。
	 */
	chrdev.dev = device_create(chrdev.class, NULL, chrdev.devid,
				   NULL, CHARDEV_DEV_NAME);
	if (!chrdev.dev)
		goto CLASS_DESTORY;

	return 0;

CLASS_DESTORY:
	class_destroy(chrdev.class);
UNREG_CHRDEV:
	unregister_chrdev(major, CHARDEV_CDEV_NAME);

	return -1;
}

static void __exit chrdev_test_exit(void)
{
	/*
	 * 释放设备。
	 */
	device_destroy(chrdev.class, MAJOR(chrdev.devid));
	/*
	 * 释放class。
	 */
	class_destroy(chrdev.class);
	/*
	 * 释放字符设备。
	 */
	unregister_chrdev(MAJOR(chrdev.devid), CHARDEV_CDEV_NAME);
}

MODULE_AUTHOR("Wangzai <1587636487@qq.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("This is chrdev test");

module_init(chrdev_test_init);
module_exit(chrdev_test_exit);
