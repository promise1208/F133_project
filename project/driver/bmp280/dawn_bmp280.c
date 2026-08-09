// SPDX-License-Identifier: GPL-2.0

/*
 * dawn_bmp280.c — BMP280 I2C 压力/温度传感器驱动
 *
 * 功能概述：
 *   - 通过 I2C 总线访问 BMP280 芯片，校验芯片 ID（0x58）
 *   - 读取出厂校准参数（存储在芯片 OTP 中的 24 字节校准数据）
 *   - 以"强制模式"（forced mode）触发单次测量，等待转换完成后读取原始 ADC 值
 *   - 用校准参数对原始 ADC 值做温度/气压补偿计算
 *   - 通过 sysfs 导出只读属性：chip_id / temperature_mdegc / pressure_pa
 *
 * 使用方式（板端）：
 *   cat /sys/bus/i2c/devices/1-0076/temperature_mdegc   # 温度，单位 m°C
 *   cat /sys/bus/i2c/devices/1-0076/pressure_pa         # 气压，单位 Pa
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/sysfs.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/slab.h>

/* 兼容不同内核版本的头文件路径：新内核用 linux/unaligned.h，旧内核用 asm/unaligned.h */
#if defined(__has_include)
#if __has_include("linux/unaligned.h")
#include <linux/unaligned.h>
#else
#include <asm/unaligned.h>
#endif
#else
#include <asm/unaligned.h>
#endif

#include "bmp280_math.h"

/* ---- 寄存器地址定义（见 BMP280 数据手册） ---- */
#define DAWN_BMP280_REG_ID          0xd0   /* 芯片 ID 寄存器，BMP280 固定为 0x58 */
#define DAWN_BMP280_REG_CALIB       0x88   /* 校准参数区起始地址（0x88 起共 24 字节） */
#define DAWN_BMP280_REG_CTRL_MEAS   0xf4   /* 测量控制寄存器：配置采样率与工作模式 */
#define DAWN_BMP280_REG_STATUS      0xf3   /* 状态寄存器：测量进行中 / NVM 拷贝进行中 */
#define DAWN_BMP280_REG_DATA        0xf7   /* 压力/温度原始数据寄存器起始地址（6 字节） */

/* ---- 常量定义 ---- */
#define DAWN_BMP280_CHIP_ID         0x58   /* BMP280 的标准芯片 ID */
#define DAWN_BMP280_CTRL_FORCED_X1  0x25   /* 强制模式、单次采样：osrs_t=1, osrs_p=1, mode=01 */
#define DAWN_BMP280_FORCED_START_DELAY_US     7000  /* 强制模式测量启动耗时（数据手册 7.3ms），最小等待 */
#define DAWN_BMP280_FORCED_START_DELAY_MAX_US 8000  /* 启动延时上限（覆盖时钟偏差） */
#define DAWN_BMP280_MEASUREMENT_TIMEOUT_MS 20       /* 等待测量完成的总超时时间 */
#define DAWN_BMP280_STATUS_IM_UPDATE      BIT(0)    /* 置 1 表示芯片正在从 NVM 拷贝校准数据 */
#define DAWN_BMP280_STATUS_MEASURING      BIT(3)    /* 置 1 表示一次测量正在进行中 */

/* 驱动私有数据结构 */
struct dawn_bmp280_data {
	struct i2c_client *client;      /* 对应的 I2C 客户端（总线号 + 设备地址 0x76） */
	struct mutex lock;              /* 互斥锁：保护测量过程不被并发 sysfs 读取打断 */
	struct dawn_bmp280_calib calib; /* 芯片出厂校准参数（温度/气压各 3/9 个系数） */
	u8 chip_id;                     /* 探测时读到的芯片 ID，用于 sysfs 展示 */
};

/*
 * 等待芯片空闲（可开始新测量 / 测量完成）。
 * conversion_started=true 时，先 sleep 覆盖强制模式的启动时间，再轮询状态寄存器。
 */
static int dawn_bmp280_wait_ready(struct dawn_bmp280_data *data,
				  bool conversion_started)
{
	unsigned long deadline;
	int status;

	/* 计算轮询超时时间点 */
	deadline = jiffies +
		   msecs_to_jiffies(DAWN_BMP280_MEASUREMENT_TIMEOUT_MS);
	if (conversion_started)
		/* 强制模式触发后，先等待芯片完成一次转换（数据手册约 7.3ms） */
		usleep_range(DAWN_BMP280_FORCED_START_DELAY_US,
			     DAWN_BMP280_FORCED_START_DELAY_MAX_US);

	for (;;) {
		/* 超时保护：总等待时间超过 20ms 则放弃 */
		if (time_after_eq(jiffies, deadline))
			return -ETIMEDOUT;

		/* 读取状态寄存器 */
		status = i2c_smbus_read_byte_data(data->client,
						 DAWN_BMP280_REG_STATUS);
		if (status < 0)
			return status;   /* I2C 通信错误 */
		if (time_after_eq(jiffies, deadline))
			return -ETIMEDOUT;
		/* 既不在拷贝校准数据、也不在测量中，说明芯片空闲，可以返回 */
		if (!(status & (DAWN_BMP280_STATUS_IM_UPDATE |
				DAWN_BMP280_STATUS_MEASURING)))
			return 0;

		/* 芯片忙，休眠 1~2ms 后继续轮询 */
		usleep_range(1000, 2000);
	}
}

/*
 * 触发一次单次测量并读取补偿后的结果。
 *
 * 输出：
 *   temperature_centi_c — 温度，单位 0.01°C（如 2955 表示 29.55°C）
 *   pressure_pa         — 气压，单位 Pa
 */
static int dawn_bmp280_measure(struct dawn_bmp280_data *data,
			       s32 *temperature_centi_c, u32 *pressure_pa)
{
	u8 raw_data[6];                 /* 原始数据：[压力3字节][温度3字节] */
	u32 adc_press;                  /* 拼装后的 20 位压力 ADC 值 */
	u32 adc_temp;                   /* 拼装后的 20 位温度 ADC 值 */
	u32 pressure_q24_8;             /* 补偿后气压：Q24.8 定点数（整数部分单位 Pa） */
	u64 pressure_pa_value;          /* 从 Q24.8 转换出的整数 Pa 值 */
	s32 temperature;                /* 补偿后温度：0.01°C */
	s32 t_fine;                     /* 温度补偿中间量 t_fine，供气压补偿使用 */
	int ret;

	if (!temperature_centi_c || !pressure_pa)
		return -EINVAL;         /* 输出指针为空，参数非法 */

	/*
	 * 写入控制寄存器，触发一次强制测量：
	 * osrs_t=1（温度 1 次采样）、osrs_p=1（气压 1 次采样）、mode=01（forced）
	 */
	ret = i2c_smbus_write_byte_data(data->client,
				       DAWN_BMP280_REG_CTRL_MEAS,
				       DAWN_BMP280_CTRL_FORCED_X1);
	if (ret < 0)
		return ret;

	/* 等待本次测量完成 */
	ret = dawn_bmp280_wait_ready(data, true);
	if (ret)
		return ret;

	/* 从 0xf7 起连续读取 6 字节原始数据 */
	ret = i2c_smbus_read_i2c_block_data(data->client,
					    DAWN_BMP280_REG_DATA,
					    sizeof(raw_data), raw_data);
	if (ret < 0)
		return ret;
	if (ret != sizeof(raw_data))
		return -EIO;            /* 读到的字节数不对 */

	/*
	 * 拼装 20 位 ADC 值：
	 * 寄存器中每个 ADC 值占 3 字节，低 4 位无效。
	 *   压力 = (raw[0]<<12) | (raw[1]<<4) | (raw[2]>>4)
	 *   温度 = (raw[3]<<12) | (raw[4]<<4) | (raw[5]>>4)
	 */
	adc_press = ((u32)raw_data[0] << 12) |
		    ((u32)raw_data[1] << 4) | (raw_data[2] >> 4);
	adc_temp = ((u32)raw_data[3] << 12) |
		   ((u32)raw_data[4] << 4) | (raw_data[5] >> 4);

	/* 用校准参数补偿温度，同时得到 t_fine（气压补偿需要用到） */
	if (!dawn_bmp280_compensate_temp(&data->calib, (s32)adc_temp,
					&temperature, &t_fine))
		return -EIO;
	/* 用校准参数补偿气压，得到 Q24.8 定点结果 */
	if (!dawn_bmp280_compensate_press(&data->calib, (s32)adc_press,
					 t_fine, &pressure_q24_8))
		return -EIO;

	/* Q24.8 → 整数 Pa：除以 256，四舍五入（+128 后 >>8） */
	pressure_pa_value = ((u64)pressure_q24_8 + 128) / 256;
	*temperature_centi_c = temperature;
	*pressure_pa = (u32)pressure_pa_value;

	return 0;
}

/* sysfs 属性：chip_id —— 显示芯片 ID，如 0x58 */
static ssize_t chip_id_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dawn_bmp280_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%02x\n", data->chip_id);
}

/* sysfs 属性：temperature_mdegc —— 显示温度，单位 m°C */
static ssize_t temperature_mdegc_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct dawn_bmp280_data *data = dev_get_drvdata(dev);
	s32 temperature_centi_c;   /* 测量结果：温度，0.01°C */
	s64 temperature_mdegc;     /* 换算结果：温度，m°C（0.001°C） */
	u32 pressure_pa;
	int ret;

	/* 加锁执行测量，避免两个 sysfs 读取并发操作 I2C */
	mutex_lock(&data->lock);
	ret = dawn_bmp280_measure(data, &temperature_centi_c, &pressure_pa);
	mutex_unlock(&data->lock);
	if (ret)
		return ret;

	/* 0.01°C → 0.001°C：乘以 10，如 2955 → 29550（即 29.55°C） */
	temperature_mdegc = (s64)temperature_centi_c * 10;
	return scnprintf(buf, PAGE_SIZE, "%lld\n",
			 (long long)temperature_mdegc);
}

/* sysfs 属性：pressure_pa —— 显示气压，单位 Pa（如 98816 即 988.16 hPa） */
static ssize_t pressure_pa_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dawn_bmp280_data *data = dev_get_drvdata(dev);
	s32 temperature_centi_c;
	u32 pressure_pa;
	int ret;

	mutex_lock(&data->lock);
	ret = dawn_bmp280_measure(data, &temperature_centi_c, &pressure_pa);
	mutex_unlock(&data->lock);
	if (ret)
		return ret;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pressure_pa);
}

/* 声明三个只读 sysfs 属性 */
static DEVICE_ATTR_RO(chip_id);
static DEVICE_ATTR_RO(temperature_mdegc);
static DEVICE_ATTR_RO(pressure_pa);

/* sysfs 属性列表（挂到设备 kobject 上） */
static struct attribute *dawn_bmp280_attrs[] = {
	&dev_attr_chip_id.attr,
	&dev_attr_temperature_mdegc.attr,
	&dev_attr_pressure_pa.attr,
	NULL,
};

static const struct attribute_group dawn_bmp280_attr_group = {
	.attrs = dawn_bmp280_attrs,
};

/*
 * 驱动探测函数：I2C 子系统在设备树节点与驱动匹配成功后调用。
 * 流程：校验适配器能力 → 读芯片 ID → 等待就绪 → 读校准数据 → 创建 sysfs。
 */
static int dawn_bmp280_probe(struct i2c_client *client,
				     const struct i2c_device_id *id)
{
	struct dawn_bmp280_data *data;
	u8 calib_data[24];   /* 24 字节原始校准数据缓冲 */
	int chip_id;
	int ret;

	/* 校验 I2C 适配器是否支持本驱动所需的 SMBus 操作 */
	if (!i2c_check_functionality(client->adapter,
				    I2C_FUNC_SMBUS_BYTE_DATA |
				    I2C_FUNC_SMBUS_I2C_BLOCK)) {
		ret = -EOPNOTSUPP;
		goto err_probe;
	}

	/* 分配驱动私有数据结构（devm_* 随设备释放自动回收） */
	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err_probe;
	}

	data->client = client;
	mutex_init(&data->lock);

	/* 读取芯片 ID 寄存器，确认挂在总线上的确实是 BMP280 */
	chip_id = i2c_smbus_read_byte_data(client, DAWN_BMP280_REG_ID);
	if (chip_id < 0) {
		ret = chip_id;
		goto err_probe;
	}
	if (chip_id != DAWN_BMP280_CHIP_ID) {
		ret = -ENODEV;   /* ID 不符，可能是其他器件 */
		goto err_probe;
	}
	data->chip_id = (u8)chip_id;

	/* 等待芯片完成上电后的 NVM 校准数据拷贝（此时不可访问校准区） */
	ret = dawn_bmp280_wait_ready(data, false);
	if (ret)
		goto err_probe;

	/* 从 0x88 起连续读取 24 字节出厂校准数据 */
	ret = i2c_smbus_read_i2c_block_data(client, DAWN_BMP280_REG_CALIB,
						    sizeof(calib_data), calib_data);
	if (ret < 0)
		goto err_probe;
	if (ret != sizeof(calib_data)) {
		ret = -EIO;
		goto err_probe;
	}

	/*
	 * 解析校准数据：
	 * 前 6 字节为温度系数 dig_t1(无符号16位)/dig_t2/dig_t3(有符号16位)；
	 * 后 18 字节为气压系数 dig_p1(无符号16位)/dig_p2~dig_p9(有符号16位)。
	 * 全部为小端序，用 get_unaligned_le16 安全读取。
	 */
	data->calib.dig_t1 = get_unaligned_le16(&calib_data[0]);
	data->calib.dig_t2 = (s16)get_unaligned_le16(&calib_data[2]);
	data->calib.dig_t3 = (s16)get_unaligned_le16(&calib_data[4]);
	data->calib.dig_p1 = get_unaligned_le16(&calib_data[6]);
	data->calib.dig_p2 = (s16)get_unaligned_le16(&calib_data[8]);
	data->calib.dig_p3 = (s16)get_unaligned_le16(&calib_data[10]);
	data->calib.dig_p4 = (s16)get_unaligned_le16(&calib_data[12]);
	data->calib.dig_p5 = (s16)get_unaligned_le16(&calib_data[14]);
	data->calib.dig_p6 = (s16)get_unaligned_le16(&calib_data[16]);
	data->calib.dig_p7 = (s16)get_unaligned_le16(&calib_data[18]);
	data->calib.dig_p8 = (s16)get_unaligned_le16(&calib_data[20]);
	data->calib.dig_p9 = (s16)get_unaligned_le16(&calib_data[22]);
	/* dig_p1 恒不为 0；读到 0 说明校准数据异常 */
	if (!data->calib.dig_p1) {
		ret = -EINVAL;
		goto err_probe;
	}

	/* 保存私有数据指针，并创建 sysfs 属性组 */
	i2c_set_clientdata(client, data);
	ret = sysfs_create_group(&client->dev.kobj, &dawn_bmp280_attr_group);
	if (ret)
		goto err_probe;

	dev_info(&client->dev, "detected BMP280 at 0x%02x, chip ID 0x%02x\n",
		 client->addr, data->chip_id);

	return 0;

err_probe:
	dev_err(&client->dev, "probe failed: %d\n", ret);
	return ret;
}

/* 驱动卸载时移除 sysfs 属性组 */
static int dawn_bmp280_remove(struct i2c_client *client)
{
	sysfs_remove_group(&client->dev.kobj, &dawn_bmp280_attr_group);
	return 0;
}

/* 设备树匹配表：设备树节点 compatible = "dawn,bmp280" 时绑定本驱动 */
static const struct of_device_id dawn_bmp280_of_match[] = {
	{ .compatible = "dawn,bmp280" },
	{ }
};
MODULE_DEVICE_TABLE(of, dawn_bmp280_of_match);

/* I2C 设备 ID 表：兼容非设备树方式实例化 */
static const struct i2c_device_id dawn_bmp280_id[] = {
	{ "dawn_bmp280", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, dawn_bmp280_id);

/* I2C 驱动注册结构 */
static struct i2c_driver dawn_bmp280_driver = {
	.driver = {
		.name = "dawn_bmp280",
		.of_match_table = dawn_bmp280_of_match,
	},
	.probe = dawn_bmp280_probe,
	.remove = dawn_bmp280_remove,
	.id_table = dawn_bmp280_id,
};

/* 模块加载/卸载时自动注册/注销 I2C 驱动 */
module_i2c_driver(dawn_bmp280_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Dawn project contributors");
MODULE_DESCRIPTION("BMP280 I2C sensor driver");
