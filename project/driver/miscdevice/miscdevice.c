// SPDX-License-Identifier: GPL-2.0
/*
 * miscdevice test
 *
 * Copyright (C) 2025 Wangzai 《搞linux的旺仔》
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/cdev.h>

#define MISCDEVICE_DEV_NAME "my_miscdevice"
#define MALLOC_LEN (4096)

/*
 * 杂设备open()函数
 */
static int miscdevice_open(struct inode *inode, struct file *filp)
{
	char *buf = NULL;

	/*
	 * 申请一段内存并保存到文件的私有数据中。
	 */
	buf = kmalloc(MALLOC_LEN, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	filp->private_data = buf;

	return 0;
}

/*
 * 杂设备release()函数，即关闭函数
 */
static int miscdevice_release(struct inode *inode, struct file *filp)
{
	char *buf = (char *)filp->private_data;

	kfree(buf);

	return 0;
}

/*
 * 杂设备读函数
 */
static ssize_t miscdevice_read(struct file *filp, char __user *ptr, size_t len,
			       loff_t *off)
{
	char *buf = (char *)filp->private_data;

	/*
	 * 通过copy_to_user()函数将内核空间的数据(buf)拷贝到应用程序空间(ptr)。
	 */
	if (copy_to_user(ptr, buf, len))
		return -EFAULT;

	return len;
}

/*
 * 杂设备写函数
 */
static ssize_t miscdevice_write(struct file *filp, const char __user *ptr,
				size_t len, loff_t *off)
{
	char *buf = (char *)filp->private_data;

	/*
	 * 通过copy_from_user()函数将应用程序空间的数据(ptr拷贝到内核空间(buf)。
	 */
	if (copy_from_user(buf,  ptr, len))
		return -EFAULT;

	return len;
}

/*
 * 杂设备iotctl()函数，通过cmd传递命令，通过arg传递数据
 */
static long miscdevice_ioctl(struct file *filp,
			     unsigned int cmd,
			     unsigned long arg)
{
	return 0;
}

/*
 * 文件操作结构体
 */
static const struct file_operations miscdevice_fops = {
	.owner = THIS_MODULE,
	.open = miscdevice_open,
	.release = miscdevice_release,
	.read = miscdevice_read,
	.write = miscdevice_write,
	.unlocked_ioctl = miscdevice_ioctl,
};

/*
 * 杂设备结构体
 */
static struct miscdevice my_miscdevice = {
	.name   = MISCDEVICE_DEV_NAME,
	.minor  = MISC_DYNAMIC_MINOR,
	.fops   = &miscdevice_fops,
};

static int __init miscdevice_test_init(void)
{
	/*
	 * 注册杂设备
	 */
	return misc_register(&my_miscdevice);
}

static void __exit miscdevice_test_exit(void)
{
	/*
	 * 注销杂设备
	 */
	misc_deregister(&my_miscdevice);
}

MODULE_AUTHOR("Wangzai <1587636487@qq.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("This is miscdevice test");

module_init(miscdevice_test_init);
module_exit(miscdevice_test_exit);
