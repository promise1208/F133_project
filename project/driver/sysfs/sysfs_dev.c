/*
 * sysfs-dev device driver
 *
 * Copyright (C) 2010 xxxx Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <linux/cdev.h>

#define CHARDEV_DEV_CNT 1
#define CHARDEV_DEV_NAME "sysfs_dev"
#define CHARDEV_CLASS_NAME "sysfs_dev"

struct sysfs_dev_dev
{
	dev_t  devid;
	struct attribute_group attrs;
	struct class  *class;
	struct device *dev;
};

static struct sysfs_dev_dev *sysfs_dev;
static ssize_t sysfs_devop_test_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	return sprintf(buf, "read\n");
}

static ssize_t sysfs_devop_test_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf,
				   size_t count)
{
	printk("write\n");

	return count;
}

static DEVICE_ATTR(sysfs_devop_test, S_IRUGO | S_IWUSR, sysfs_devop_test_show, sysfs_devop_test_store);

static struct attribute *sysfs_devop_attributes[] = {
	&dev_attr_sysfs_devop_test.attr,
	NULL
};

static const struct attribute_group sysfs_devop_attr_group = {
	.attrs = sysfs_devop_attributes,
};


static int sysfs_dev_init(void)
{
	int retval = 0;

	sysfs_dev = kzalloc(sizeof(*sysfs_dev), GFP_KERNEL);
	if (!sysfs_dev)
		goto ret;
	
	sysfs_dev->class = class_create(THIS_MODULE, CHARDEV_CLASS_NAME);
	if (IS_ERR(sysfs_dev->class)) {
		retval = PTR_ERR(sysfs_dev->class);
		goto err_class_create;
	}

	retval = alloc_sysfs_dev_region(&sysfs_dev->devid, 0, CHARDEV_DEV_CNT,
				     CHARDEV_DEV_NAME);
	if (retval)
		goto err_alloc_sysfs_dev_region;

	sysfs_dev->dev = device_create(sysfs_dev->class, NULL,
				    sysfs_dev->devid, 
				    NULL,
				    CHARDEV_DEV_NAME);
	if (IS_ERR(sysfs_dev->dev)) {
		retval = PTR_ERR(sysfs_dev->dev);
		goto err_device_create;
	} 

	retval = sysfs_create_group(&sysfs_dev->dev->kobj, &sysfs_devop_attr_group);
	if (retval) {
		pr_err("Failed to create sysfs_devop attribute group\n");
		goto err_sysfs_create_group;
	}

	return 0;

err_sysfs_create_group:
	device_destroy(sysfs_dev->class, sysfs_dev->devid);
err_device_create:
	unregister_sysfs_dev_region(sysfs_dev->devid, CHARDEV_DEV_CNT);
err_alloc_sysfs_dev_region:
	class_destroy(sysfs_dev->class);
err_class_create:
	kfree(sysfs_dev);
ret:
	pr_err("sysfs_dev_init fail\n");
	return retval;
}

static void sysfs_dev_exit(void)
{
	sysfs_remove_group(&sysfs_dev->dev->kobj, &sysfs_devop_attr_group);
	device_destroy(sysfs_dev->class, sysfs_dev->devid);
	class_destroy(sysfs_dev->class);
	unregister_sysfs_dev_region(sysfs_dev->devid, CHARDEV_DEV_CNT);
	kfree(sysfs_dev);
}

module_init(sysfs_dev_init);
module_exit(sysfs_dev_exit);

MODULE_DESCRIPTION("sysfs_dev driver");
MODULE_LICENSE("GPL");
//MODULE_INFO(intree, "Y");
