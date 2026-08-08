/* SPDX-License-Identifier: GPL-2.0 */

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

static inline bool dawn_bmp280_add_s64(dawn_bmp280_s64 a,
					       dawn_bmp280_s64 b,
					       dawn_bmp280_s64 *result)
{
	return __builtin_add_overflow(a, b, result);
}

static inline bool dawn_bmp280_sub_s64(dawn_bmp280_s64 a,
					       dawn_bmp280_s64 b,
					       dawn_bmp280_s64 *result)
{
	return __builtin_sub_overflow(a, b, result);
}

static inline bool dawn_bmp280_mul_s64(dawn_bmp280_s64 a,
					       dawn_bmp280_s64 b,
					       dawn_bmp280_s64 *result)
{
	return __builtin_mul_overflow(a, b, result);
}

static inline bool dawn_bmp280_div_s64(dawn_bmp280_s64 dividend,
					       dawn_bmp280_s64 divisor,
					       dawn_bmp280_s64 *result)
{
	if (divisor == 0 ||
	    (dividend == (-9223372036854775807LL - 1) && divisor == -1))
		return true;

	*result = dividend / divisor;
	return false;
}

static inline bool dawn_bmp280_fits_s32(dawn_bmp280_s64 value)
{
	return value >= -2147483648LL && value <= 2147483647LL;
}

static inline bool dawn_bmp280_compensate_temp(
	const struct dawn_bmp280_calib *calib, dawn_bmp280_s32 adc_temp,
	dawn_bmp280_s32 *temperature_centi_c, dawn_bmp280_s32 *t_fine)
{
	dawn_bmp280_s64 var1, var2, term, t_fine_value, temp_value;

	if (calib == 0 || temperature_centi_c == 0 || t_fine == 0)
		return false;
	if (adc_temp < 0 || adc_temp > 1048575)
		return false;

	var1 = (dawn_bmp280_s64)(adc_temp >> 3);
	term = (dawn_bmp280_s64)calib->dig_t1;
	if (dawn_bmp280_mul_s64(term, 2, &term) ||
	    dawn_bmp280_sub_s64(var1, term, &var1) ||
	    dawn_bmp280_mul_s64(var1, calib->dig_t2, &term))
		return false;
	var1 = term >> 11;

	var2 = (dawn_bmp280_s64)(adc_temp >> 4);
	if (dawn_bmp280_sub_s64(var2, calib->dig_t1, &var2) ||
	    dawn_bmp280_mul_s64(var2, var2, &term))
		return false;
	var2 = term >> 12;
	if (dawn_bmp280_mul_s64(var2, calib->dig_t3, &term))
		return false;
	var2 = term >> 14;

	if (dawn_bmp280_add_s64(var1, var2, &t_fine_value) ||
	    dawn_bmp280_mul_s64(t_fine_value, 5, &temp_value) ||
	    dawn_bmp280_add_s64(temp_value, 128, &temp_value))
		return false;
	temp_value >>= 8;

	if (!dawn_bmp280_fits_s32(t_fine_value) ||
	    !dawn_bmp280_fits_s32(temp_value))
		return false;

	*temperature_centi_c = (dawn_bmp280_s32)temp_value;
	*t_fine = (dawn_bmp280_s32)t_fine_value;
	return true;
}

static inline bool dawn_bmp280_compensate_press(
	const struct dawn_bmp280_calib *calib, dawn_bmp280_s32 adc_press,
	dawn_bmp280_s32 t_fine, dawn_bmp280_u32 *pressure_q24_8)
{
	dawn_bmp280_s64 var1, var2, p, term, var1_sq;

	if (calib == 0 || pressure_q24_8 == 0)
		return false;
	if (adc_press < 0 || adc_press > 1048575)
		return false;

	if (dawn_bmp280_sub_s64(t_fine, 128000, &var1) ||
	    dawn_bmp280_mul_s64(var1, var1, &var1_sq) ||
	    dawn_bmp280_mul_s64(var1_sq, calib->dig_p6, &var2) ||
	    dawn_bmp280_mul_s64(var1, calib->dig_p5, &term) ||
	    dawn_bmp280_mul_s64(term, 131072, &term) ||
	    dawn_bmp280_add_s64(var2, term, &var2) ||
	    dawn_bmp280_mul_s64(calib->dig_p4, 34359738368LL, &term) ||
	    dawn_bmp280_add_s64(var2, term, &var2))
		return false;

	if (dawn_bmp280_mul_s64(var1_sq, calib->dig_p3, &term))
		return false;
	term >>= 8;
	if (dawn_bmp280_mul_s64(var1, calib->dig_p2, &p) ||
	    dawn_bmp280_mul_s64(p, 4096, &p) ||
	    dawn_bmp280_add_s64(term, p, &var1))
		return false;

	if (dawn_bmp280_add_s64(140737488355328LL, var1, &term) ||
	    dawn_bmp280_mul_s64(term, calib->dig_p1, &term))
		return false;
	var1 = term >> 33;

	if (var1 == 0)
		return false;

	if (dawn_bmp280_sub_s64(1048576, adc_press, &term) ||
	    dawn_bmp280_mul_s64(term, 2147483648LL, &term) ||
	    dawn_bmp280_sub_s64(term, var2, &term) ||
	    dawn_bmp280_mul_s64(term, 3125, &p) ||
	    dawn_bmp280_div_s64(p, var1, &p))
		return false;

	term = p >> 13;
	if (dawn_bmp280_mul_s64(calib->dig_p9, term, &var1) ||
	    dawn_bmp280_mul_s64(var1, term, &var1))
		return false;
	var1 >>= 25;

	if (dawn_bmp280_mul_s64(calib->dig_p8, p, &var2))
		return false;
	var2 >>= 19;

	if (dawn_bmp280_add_s64(p, var1, &term) ||
	    dawn_bmp280_add_s64(term, var2, &term))
		return false;
	term >>= 8;
	if (dawn_bmp280_mul_s64(calib->dig_p7, 16, &p) ||
	    dawn_bmp280_add_s64(term, p, &p))
		return false;

	if (p < 0 || p > (dawn_bmp280_s64)0xffffffff)
		return false;

	*pressure_q24_8 = (dawn_bmp280_u32)p;
	return true;
}

#endif
