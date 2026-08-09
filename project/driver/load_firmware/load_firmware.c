// SPDX-License-Identifier: GPL-2.0
/*
 * load firmware test
 *
 * Copyright (C) 2025 Wangzai 《搞linux的旺仔》
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>

/*
 * 在根文件系统的/lib/firmware/下要放一个名字为LOAD_IMAGE_NAME所定义的文件
 */
#define LOAD_IMAGE_NAME "test.firmware"

static struct miscdevice load_firmware_misc_dev = {
	.name   = "load_firmware_test",
	.minor  = MISC_DYNAMIC_MINOR,
};

static int __init load_firmware_test_init(void)
{
	const struct firmware *fw;
	int ret;

	/*
	 * 注册一个杂设备
	 */
	ret = misc_register(&load_firmware_misc_dev);
	if (ret)
		return ret;

	/*
	 * 利用request_firmware()加载文件系统下的固件
	 */
	ret = request_firmware(&fw, LOAD_IMAGE_NAME, load_firmware_misc_dev.this_device);
	if (ret) {
		misc_deregister(&load_firmware_misc_dev);
		return ret;
	}

	pr_info("firmware = %s\n", fw->data);

	/*
	 * 释放固件
	 */
	release_firmware(fw);

	return 0;
}

static void __exit load_firmware_test_exit(void)
{
	/*
	 * 注销杂设备
	 */
	misc_deregister(&load_firmware_misc_dev);
}

MODULE_AUTHOR("Wangzai <1587636487@qq.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("This is load firmware test");

module_init(load_firmware_test_init);
module_exit(load_firmware_test_exit);

