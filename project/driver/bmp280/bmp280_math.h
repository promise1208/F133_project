#ifndef DAWN_BMP280_MATH_H
#define DAWN_BMP280_MATH_H

#ifdef __KERNEL__
#include <linux/types.h>

typedef u16 dawn_bmp280_u16;
typedef s16 dawn_bmp280_s16;
typedef s32 dawn_bmp280_s32;
typedef s64 dawn_bmp280_s64;
typedef u32 dawn_bmp280_u32;
#else
#include <stdbool.h>
#include <stdint.h>

typedef uint16_t dawn_bmp280_u16;
typedef int16_t dawn_bmp280_s16;
typedef int32_t dawn_bmp280_s32;
typedef int64_t dawn_bmp280_s64;
typedef uint32_t dawn_bmp280_u32;
#endif

struct dawn_bmp280_calib {
	dawn_bmp280_u16 dig_t1;
	dawn_bmp280_s16 dig_t2;
	dawn_bmp280_s16 dig_t3;
	dawn_bmp280_u16 dig_p1;
	dawn_bmp280_s16 dig_p2;
	dawn_bmp280_s16 dig_p3;
	dawn_bmp280_s16 dig_p4;
	dawn_bmp280_s16 dig_p5;
	dawn_bmp280_s16 dig_p6;
	dawn_bmp280_s16 dig_p7;
	dawn_bmp280_s16 dig_p8;
	dawn_bmp280_s16 dig_p9;
};

static inline dawn_bmp280_s32 dawn_bmp280_compensate_temp(
	const struct dawn_bmp280_calib *calib, dawn_bmp280_s32 adc_temp,
	dawn_bmp280_s32 *t_fine)
{
	dawn_bmp280_s32 var1, var2;

	var1 = (((adc_temp >> 3) - ((dawn_bmp280_s32)calib->dig_t1 << 1)) *
		(dawn_bmp280_s32)calib->dig_t2) >> 11;
	var2 = (((((adc_temp >> 4) - (dawn_bmp280_s32)calib->dig_t1) *
		  ((adc_temp >> 4) - (dawn_bmp280_s32)calib->dig_t1)) >> 12) *
		(dawn_bmp280_s32)calib->dig_t3) >> 14;
	*t_fine = var1 + var2;

	return (*t_fine * 5 + 128) >> 8;
}

static inline bool dawn_bmp280_compensate_press(
	const struct dawn_bmp280_calib *calib, dawn_bmp280_s32 adc_press,
	dawn_bmp280_s32 t_fine, dawn_bmp280_u32 *pressure_q24_8)
{
	dawn_bmp280_s64 var1, var2, p;

	if (calib == 0 || pressure_q24_8 == 0)
		return false;

	var1 = (dawn_bmp280_s64)t_fine - 128000;
	var2 = var1 * var1 * (dawn_bmp280_s64)calib->dig_p6;
	var2 += (var1 * (dawn_bmp280_s64)calib->dig_p5) * 131072;
	var2 += (dawn_bmp280_s64)calib->dig_p4 * 34359738368;
	var1 = ((var1 * var1 * (dawn_bmp280_s64)calib->dig_p3) >> 8) +
		((var1 * (dawn_bmp280_s64)calib->dig_p2) * 4096);
	var1 = ((((dawn_bmp280_s64)1 << 47) + var1) *
		(dawn_bmp280_s64)calib->dig_p1) >> 33;

	if (var1 == 0)
		return false;

	p = ((((dawn_bmp280_s64)1048576 - adc_press) * 2147483648) - var2) *
		3125;
	p /= var1;
	var1 = ((dawn_bmp280_s64)calib->dig_p9 * (p >> 13) *
		(p >> 13)) >> 25;
	var2 = ((dawn_bmp280_s64)calib->dig_p8 * p) >> 19;
	p = ((p + var1 + var2) >> 8) +
		((dawn_bmp280_s64)calib->dig_p7 * 16);

	if (p < 0 || p > (dawn_bmp280_s64)0xffffffff)
		return false;

	*pressure_q24_8 = (dawn_bmp280_u32)p;
	return true;
}

#endif
