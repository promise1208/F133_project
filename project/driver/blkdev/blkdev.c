// SPDX-License-Identifier: GPL-2.0
/*
 * block test
 *
 * Copyright (C) 2025 Wangzai 《搞linux的旺仔》
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/blk-mq.h>

#define RAM_LEN			(4096*200)
#define RAM_SECTORS		(RAM_LEN / SECTOR_SIZE)
#define RAM_MAJOR               (37)
#define WZB_NAME		"wzblk"

struct wzb_device {
	struct request_queue *wzb_queue;
	struct blk_mq_tag_set tag_set;
	struct gendisk *wzb_disk;
	spinlock_t lock;
	char *ram_mem;
};

struct wzb_device *wzb;

/*
 * 块设备队列请求处理函数。
 */
static blk_status_t wzb_queue_rq(struct blk_mq_hw_ctx *hctx,
				 const struct blk_mq_queue_data *bd)
{

	struct wzb_device *wzb = hctx->queue->queuedata;
	unsigned int cnt, curr_sector;
	struct request *req = bd->rq;
	unsigned long flags;
	int err = BLK_STS_OK;
	void *data;

	/* 块设备请求从调动器移交的设备驱动层。 */
	blk_mq_start_request(req);

	/* 上锁，避免竞态。 */
	spin_lock_irqsave(&wzb->lock, flags);

	do {
		/*
		 * blk_rq_cur_sectors(req):获取请求的扇区数。
		 * blk_rq_pos(req):获取请求的首个扇区号。
		 */
		for (cnt = 0; cnt < blk_rq_cur_sectors(req); cnt++) {
			/* 计算当前扇区号。 */
			curr_sector = blk_rq_pos(req) + cnt;
			/* 获取要写给块设备或被块设备写的内存地址。 */
			data = bio_data(req->bio) + SECTOR_SIZE * cnt;
			if (rq_data_dir(req) == READ)
			/*
			 * 从块设备读。这里通过wzb->ram_mem[]模拟磁盘空间，
			 * 即从wzb->ram_mem[]中读。
			 */
				memcpy(data, wzb->ram_mem + (curr_sector * SECTOR_SIZE), SECTOR_SIZE);
			else
			/*
			 * 写给块设备。这里通过wzb->ram_mem[]模拟磁盘空间，
			 * 即往wzb->ram_mem[]中写。
			 */
				memcpy(wzb->ram_mem + (curr_sector * SECTOR_SIZE), data, SECTOR_SIZE);
		}
	/* blk_update_request(req, err, blk_rq_cur_bytes(req)):更新已经请求的剩余数据。 */
	} while (blk_update_request(req, err, blk_rq_cur_bytes(req)));

	/* 解锁。 */
	spin_unlock_irqrestore(&wzb->lock, flags);

	/* 表面请求已经完成。 */
	blk_mq_end_request(req, BLK_STS_OK);

	return BLK_STS_OK;
}

/*
 * 块设备打开函数。
 */
static int wzb_open(struct block_device *bdev, fmode_t mode)
{
	pr_info("%s, %d\n", __func__, __LINE__);
	return 0;
}

/*
 * 块设备关闭函数。
 */
static void wzb_release(struct gendisk *disk, fmode_t mode)
{
	pr_info("%s, %d\n", __func__, __LINE__);
}

/*
 * 块设备操作结构体。
 */
static const struct block_device_operations wzb_fops = {
	.owner		= THIS_MODULE,
	.open		= wzb_open,
	.release	= wzb_release,
};

/*
 * 块设备请求队列操作结构体。
 */
static const struct blk_mq_ops wzb_mq_ops = {
	.queue_rq	= wzb_queue_rq,
};

/*
 * 注册一个disk并添加至块设备队列。
 */
static struct wzb_device *wzb_add(void)
{
	struct wzb_device *wzb;
	struct gendisk *disk;
	int err;

	/*
	 * 申请块设备参数结构体。
	 */
	wzb = kzalloc(sizeof(struct wzb_device), GFP_KERNEL);
	if (!wzb)
		goto out;


	/* 块设备操作函数。 */
	wzb->tag_set.ops = &wzb_mq_ops;
	/* 块设备支持的硬件队列数。 */
	wzb->tag_set.nr_hw_queues = 1;
	/* 单个硬件队列嫩缓存的队列深度。 */
	wzb->tag_set.queue_depth = 128;
	/* 不绑定特定的NUMA。 */
	wzb->tag_set.numa_node = NUMA_NO_NODE;
	/* 允许块设备对相邻的io请求做合并。 */
	wzb->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
	wzb->tag_set.driver_data = wzb;

	/*
	 * 为块设备请求队列申请一个标签。
	 */
	err = blk_mq_alloc_tag_set(&wzb->tag_set);
	if (err)
		goto out_free_dev;

	/*
	 * 为块设备分配并初始化一个请求队列。
	 */
	wzb->wzb_queue = blk_mq_init_queue(&wzb->tag_set);
	if (IS_ERR(wzb->wzb_queue))
		goto out_cleanup_tags;
	wzb->wzb_queue->queuedata = wzb;

	/*
	 * 设置单次IO请求支持的最大扇区数为BLK_DEF_MAX_SECTORS。
	 */
	blk_queue_max_hw_sectors(wzb->wzb_queue, BLK_DEF_MAX_SECTORS);

	/*
	 * 申请一个disk.
	 */
	disk = wzb->wzb_disk = alloc_disk(1);
	if (!disk)
		goto out_free_queue;

	/* disk的主设备号是RAM_MAJOR。 */
	disk->major		= RAM_MAJOR;
	/* disk的第1个次设备号为1。 */
	disk->first_minor	= 1;
	/* disk操作结构体。 */
	disk->fops		= &wzb_fops;
	/* disk私有数据为wzb。*/
	disk->private_data	= wzb;
	/* 将disk与块设备请求队列绑定。 */
	disk->queue		= wzb->wzb_queue;
	/* disk的名字为WZB_NAME。 */
	sprintf(disk->disk_name, WZB_NAME);
	/* 添加disk。 */
	add_disk(disk);
	/* 设置disk的最大扇区数为RAM_SECTORS。 */
	set_capacity(disk, RAM_SECTORS);

	/* 初始化原子锁。 */
	spin_lock_init(&wzb->lock);

	return wzb;

out_free_queue:
	/* 清除块设备请求队列。 */
	blk_cleanup_queue(wzb->wzb_queue);
out_cleanup_tags:
	/* 释放块设备请求队列tag。 */
	blk_mq_free_tag_set(&wzb->tag_set);
out_free_dev:
	/* 释放块设备参数结构体。 */
	kfree(wzb);
out:
	return NULL;
}

static void wzb_remove(struct wzb_device *wzb)
{
	/* 移除disk相关结构体。 */
	del_gendisk(wzb->wzb_disk);
	/* 清除块设备请求队列。 */
	blk_cleanup_queue(wzb->wzb_queue);
	/* 释放块设备请求队列tag。 */
	blk_mq_free_tag_set(&wzb->tag_set);
	/* 减少disk的引用计数。 */
	put_disk(wzb->wzb_disk);
	/* 释放块设备参数结构体。 */
	kfree(wzb);
}

/*
 * 创建块设备的回调函数。
 */
static struct kobject *wzb_probe(dev_t dev, int *part, void *data)
{
	struct kobject *kobj = NULL;

	/*
	 * 注册一个disk并添加至块设备请求队列。
	 */
	wzb = wzb_add();
	if (wzb) {
		/*
		 * 获取块设备的kobj。
		 */
		kobj = get_disk_and_module(wzb->wzb_disk);
		/*
		 * 申请块设备访问的内存，对块设备的读写将操作这片内存。
		 */
		wzb->ram_mem = kzalloc(RAM_LEN, GFP_KERNEL);
		if (!wzb->ram_mem)
			kobj = NULL;
	}

	*part = 0;
	return kobj;
}

/*
 * 块设备初始化函数。
 */
static int __init wzb_init(void)
{
	/*
	 * 注册一个块设备， 块设备的主设备号为RAM_MAJOR,
	 * 块设备名为WZB_NAME。
	 */
	if (register_blkdev(RAM_MAJOR, WZB_NAME))
		return -EIO;

	/*
	 * 将设备号与驱动相关联。
	 * MKDEV(RAM_MAJOR, 0)：起始设备号。
	 * 1UL << MINORBITS：次设备号范围。
	 * THIS_MDULE:所属内核模块指针。
	 * wzb_probe:当设备号未被占用时，用于动态创建设备的kobject对象。
	 */
	blk_register_region(MKDEV(RAM_MAJOR, 0), 1UL << MINORBITS,
				  THIS_MODULE, wzb_probe, NULL, NULL);

	/*
	 * 注册一个disk并添加至块设备请求队列。
	 */
	wzb = wzb_add();
	if (wzb) {
		/*
		 * 申请块设备访问的内存，对块设备的读写将操作这片内存。
		 */
		wzb->ram_mem = kzalloc(RAM_LEN, GFP_KERNEL);
		if (!wzb->ram_mem)
			return -1;
	} else
		return -1;

	return 0;

}

/*
 * 块设备退出函数。
 */
static void __exit wzb_exit(void)
{
	/*
	 * 释放块设备访问的内存。
	 */
	kfree(wzb->ram_mem);

	/*
	 * 移除disk。
	 */
	wzb_remove(wzb);

	/*
	 * 注销设备号与块设备驱动的对应关系。
	 */
	blk_unregister_region(MKDEV(RAM_MAJOR, 0), 1UL << MINORBITS);

	/*
	 * 注销块设备。
	 */
	unregister_blkdev(RAM_MAJOR, WZB_NAME);
}

MODULE_AUTHOR("Wangzai <1587636487@qq.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("This is block device test");

module_init(wzb_init);
module_exit(wzb_exit);
