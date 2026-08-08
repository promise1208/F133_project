// SPDX-License-Identifier: GPL-2.0

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/sysfs.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/slab.h>

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

#define DAWN_BMP280_REG_ID          0xd0
#define DAWN_BMP280_REG_CALIB       0x88
#define DAWN_BMP280_REG_CTRL_MEAS   0xf4
#define DAWN_BMP280_REG_STATUS      0xf3
#define DAWN_BMP280_REG_DATA        0xf7
#define DAWN_BMP280_CHIP_ID         0x58
#define DAWN_BMP280_CTRL_FORCED_X1  0x25
#define DAWN_BMP280_STATUS_MEASURING BIT(3)

struct dawn_bmp280_data {
	struct i2c_client *client;
	struct mutex lock;
	struct dawn_bmp280_calib calib;
	u8 chip_id;
};

static int dawn_bmp280_measure(struct dawn_bmp280_data *data,
			       s32 *temperature_centi_c, u32 *pressure_pa)
{
	u8 raw_data[6];
	unsigned long deadline;
	u32 adc_press;
	u32 adc_temp;
	u32 pressure_q24_8;
	u64 pressure_pa_value;
	s32 temperature;
	s32 t_fine;
	int status;
	int ret;

	if (!temperature_centi_c || !pressure_pa)
		return -EINVAL;

	ret = i2c_smbus_write_byte_data(data->client,
				       DAWN_BMP280_REG_CTRL_MEAS,
				       DAWN_BMP280_CTRL_FORCED_X1);
	if (ret < 0)
		return ret;

	deadline = jiffies + msecs_to_jiffies(20);
	for (;;) {
		if (time_after_eq(jiffies, deadline))
			return -ETIMEDOUT;

		status = i2c_smbus_read_byte_data(data->client,
						 DAWN_BMP280_REG_STATUS);
		if (status < 0)
			return status;
		if (!(status & DAWN_BMP280_STATUS_MEASURING))
			break;

		if (time_after_eq(jiffies, deadline))
			return -ETIMEDOUT;
		usleep_range(1000, 2000);
	}

	ret = i2c_smbus_read_i2c_block_data(data->client,
					    DAWN_BMP280_REG_DATA,
					    sizeof(raw_data), raw_data);
	if (ret < 0)
		return ret;
	if (ret != sizeof(raw_data))
		return -EIO;

	adc_press = ((u32)raw_data[0] << 12) |
		    ((u32)raw_data[1] << 4) | (raw_data[2] >> 4);
	adc_temp = ((u32)raw_data[3] << 12) |
		   ((u32)raw_data[4] << 4) | (raw_data[5] >> 4);

	if (!dawn_bmp280_compensate_temp(&data->calib, (s32)adc_temp,
					&temperature, &t_fine))
		return -EIO;
	if (!dawn_bmp280_compensate_press(&data->calib, (s32)adc_press,
					 t_fine, &pressure_q24_8))
		return -EIO;

	pressure_pa_value = ((u64)pressure_q24_8 + 128) / 256;
	*temperature_centi_c = temperature;
	*pressure_pa = (u32)pressure_pa_value;

	return 0;
}

static ssize_t chip_id_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dawn_bmp280_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%02x\n", data->chip_id);
}

static ssize_t temperature_mdegc_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct dawn_bmp280_data *data = dev_get_drvdata(dev);
	s32 temperature_centi_c;
	s64 temperature_mdegc;
	u32 pressure_pa;
	int ret;

	mutex_lock(&data->lock);
	ret = dawn_bmp280_measure(data, &temperature_centi_c, &pressure_pa);
	mutex_unlock(&data->lock);
	if (ret)
		return ret;

	temperature_mdegc = (s64)temperature_centi_c * 10;
	return scnprintf(buf, PAGE_SIZE, "%lld\n",
			 (long long)temperature_mdegc);
}

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

static DEVICE_ATTR_RO(chip_id);
static DEVICE_ATTR_RO(temperature_mdegc);
static DEVICE_ATTR_RO(pressure_pa);

static struct attribute *dawn_bmp280_attrs[] = {
	&dev_attr_chip_id.attr,
	&dev_attr_temperature_mdegc.attr,
	&dev_attr_pressure_pa.attr,
	NULL,
};

static const struct attribute_group dawn_bmp280_attr_group = {
	.attrs = dawn_bmp280_attrs,
};

static int dawn_bmp280_probe(struct i2c_client *client,
				     const struct i2c_device_id *id)
{
	struct dawn_bmp280_data *data;
	u8 calib_data[24];
	int chip_id;
	int ret;

	if (!i2c_check_functionality(client->adapter,
				    I2C_FUNC_SMBUS_BYTE_DATA |
				    I2C_FUNC_SMBUS_I2C_BLOCK))
		return -EOPNOTSUPP;

	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->client = client;
	mutex_init(&data->lock);

	chip_id = i2c_smbus_read_byte_data(client, DAWN_BMP280_REG_ID);
	if (chip_id < 0)
		return chip_id;
	if (chip_id != DAWN_BMP280_CHIP_ID)
		return -ENODEV;
	data->chip_id = (u8)chip_id;

	ret = i2c_smbus_read_i2c_block_data(client, DAWN_BMP280_REG_CALIB,
					    sizeof(calib_data), calib_data);
	if (ret < 0)
		return ret;
	if (ret != sizeof(calib_data))
		return -EIO;

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

	i2c_set_clientdata(client, data);
	ret = sysfs_create_group(&client->dev.kobj, &dawn_bmp280_attr_group);
	if (ret)
		return ret;

	dev_info(&client->dev, "detected BMP280 at 0x%02x, chip ID 0x%02x\n",
		 client->addr, data->chip_id);

	return 0;
}

static int dawn_bmp280_remove(struct i2c_client *client)
{
	sysfs_remove_group(&client->dev.kobj, &dawn_bmp280_attr_group);
	return 0;
}

static const struct of_device_id dawn_bmp280_of_match[] = {
	{ .compatible = "dawn,bmp280" },
	{ }
};
MODULE_DEVICE_TABLE(of, dawn_bmp280_of_match);

static const struct i2c_device_id dawn_bmp280_id[] = {
	{ "dawn_bmp280", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, dawn_bmp280_id);

static struct i2c_driver dawn_bmp280_driver = {
	.driver = {
		.name = "dawn_bmp280",
		.of_match_table = dawn_bmp280_of_match,
	},
	.probe = dawn_bmp280_probe,
	.remove = dawn_bmp280_remove,
	.id_table = dawn_bmp280_id,
};

module_i2c_driver(dawn_bmp280_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Dawn project contributors");
MODULE_DESCRIPTION("BMP280 I2C sensor driver");
