/* SPDX-License-Identifier: GPL-2.0 */

/*
 * bmp280_math.h — BMP280 温度/气压补偿计算（纯头文件实现）
 *
 * 计算流程与公式严格遵循 BMP280 数据手册（Datasheet 第 4.2 节）：
 *   - dawn_bmp280_compensate_temp():  由温度 ADC 值计算温度（0.01°C）及中间量 t_fine
 *   - dawn_bmp280_compensate_press(): 由气压 ADC 值 + t_fine 计算气压（Q24.8 定点数）
 *
 * 安全设计：
 *   所有算术都经过溢出检查的 64 位有符号运算（__builtin_*_overflow），
 *   任何一步溢出或除零都会返回 false，由调用方判定为测量失败，
 *   避免静默产生错误读数。
 *
 * 兼容性：
 *   __KERNEL__ 环境下使用内核数据类型（u16/s16/...），
 *   否则使用标准 C 类型（uint16_t/...），便于在用户态做单元测试
 *   （见 tests/test_bmp280_math.c）。
 */

#ifndef DAWN_BMP280_MATH_H
#define DAWN_BMP280_MATH_H

#ifdef __KERNEL__
#include <linux/types.h>

/* 内核态：直接映射到内核固定宽度类型 */
typedef u16 dawn_bmp280_u16;
typedef s16 dawn_bmp280_s16;
typedef s32 dawn_bmp280_s32;
typedef s64 dawn_bmp280_s64;
typedef u32 dawn_bmp280_u32;
#else
#include <stdbool.h>
#include <stdint.h>

/* 用户态：映射到标准 C 固定宽度类型，保证测试环境一致 */
typedef uint16_t dawn_bmp280_u16;
typedef int16_t dawn_bmp280_s16;
typedef int32_t dawn_bmp280_s32;
typedef int64_t dawn_bmp280_s64;
typedef uint32_t dawn_bmp280_u32;
#endif

/*
 * 出厂校准参数结构（每颗芯片独立烧录，probe 时从 0x88 寄存器区读出）：
 *   dig_t1..dig_t3 — 温度补偿系数（t1 无符号，t2/t3 有符号）
 *   dig_p1..dig_p9 — 气压补偿系数（p1 无符号，p2~p9 有符号）
 */
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

/* ---- 溢出安全的 64 位算术辅助函数 ----
 * 返回 true 表示发生溢出（结果未写入），false 表示计算成功。
 * 溢出时不修改 *result，保证公式中间值不会静默出错。
 */

/* 有符号 64 位加法，溢出检测 */
static inline bool dawn_bmp280_add_s64(dawn_bmp280_s64 a,
					       dawn_bmp280_s64 b,
					       dawn_bmp280_s64 *result)
{
	return __builtin_add_overflow(a, b, result);
}

/* 有符号 64 位减法，溢出检测 */
static inline bool dawn_bmp280_sub_s64(dawn_bmp280_s64 a,
					       dawn_bmp280_s64 b,
					       dawn_bmp280_s64 *result)
{
	return __builtin_sub_overflow(a, b, result);
}

/* 有符号 64 位乘法，溢出检测 */
static inline bool dawn_bmp280_mul_s64(dawn_bmp280_s64 a,
					       dawn_bmp280_s64 b,
					       dawn_bmp280_s64 *result)
{
	return __builtin_mul_overflow(a, b, result);
}

/* 有符号 64 位除法：除零或 INT64_MIN / -1 视为错误（返回 true） */
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

/* 判断 64 位值能否无损放入 32 位有符号整数 */
static inline bool dawn_bmp280_fits_s32(dawn_bmp280_s64 value)
{
	return value >= -2147483648LL && value <= 2147483647LL;
}

/*
 * 温度补偿计算（数据手册公式）：
 *   var1  = (adc_temp/8 - dig_t1*2) * dig_t2 / 2048
 *   var2  = (adc_temp/16 - dig_t1)^2 * dig_t3 / 16384
 *   t_fine = var1 + var2
 *   温度(0.01°C) = (t_fine*5 + 128) / 256
 *
 * 其中 adc_temp 为 20 位无符号 ADC 原始值（0~1048575）。
 *
 * 输出：
 *   temperature_centi_c — 温度，单位 0.01°C
 *   t_fine              — 中间量，供气压补偿使用
 * 返回 false 表示输入非法或运算溢出。
 */
static inline bool dawn_bmp280_compensate_temp(
	const struct dawn_bmp280_calib *calib, dawn_bmp280_s32 adc_temp,
	dawn_bmp280_s32 *temperature_centi_c, dawn_bmp280_s32 *t_fine)
{
	dawn_bmp280_s64 var1, var2, term, t_fine_value, temp_value;

	/* 参数合法性检查 */
	if (calib == 0 || temperature_centi_c == 0 || t_fine == 0)
		return false;
	if (adc_temp < 0 || adc_temp > 1048575)
		return false;

	/* var1 = (adc_temp/8 - dig_t1*2) * dig_t2 / 2048 */
	var1 = (dawn_bmp280_s64)(adc_temp >> 3);
	term = (dawn_bmp280_s64)calib->dig_t1;
	if (dawn_bmp280_mul_s64(term, 2, &term) ||
	    dawn_bmp280_sub_s64(var1, term, &var1) ||
	    dawn_bmp280_mul_s64(var1, calib->dig_t2, &term))
		return false;
	var1 = term >> 11;

	/* var2 = (adc_temp/16 - dig_t1)^2 * dig_t3 / 16384 */
	var2 = (dawn_bmp280_s64)(adc_temp >> 4);
	if (dawn_bmp280_sub_s64(var2, calib->dig_t1, &var2) ||
	    dawn_bmp280_mul_s64(var2, var2, &term))
		return false;
	var2 = term >> 12;
	if (dawn_bmp280_mul_s64(var2, calib->dig_t3, &term))
		return false;
	var2 = term >> 14;

	/* t_fine = var1 + var2；温度 = (t_fine*5 + 128) >> 8 */
	if (dawn_bmp280_add_s64(var1, var2, &t_fine_value) ||
	    dawn_bmp280_mul_s64(t_fine_value, 5, &temp_value) ||
	    dawn_bmp280_add_s64(temp_value, 128, &temp_value))
		return false;
	temp_value >>= 8;

	/* 结果必须能放入 32 位有符号整数 */
	if (!dawn_bmp280_fits_s32(t_fine_value) ||
	    !dawn_bmp280_fits_s32(temp_value))
		return false;

	*temperature_centi_c = (dawn_bmp280_s32)temp_value;
	*t_fine = (dawn_bmp280_s32)t_fine_value;
	return true;
}

/*
 * 气压补偿计算（数据手册公式，使用 Q24.8 定点数避免浮点运算）：
 *
 *   var1 = (t_fine/2 - 64000)^2 * dig_p6 / 2^40
 *        + (t_fine/2 - 64000) * dig_p5 * 2
 *        + dig_p4 * 2^35
 *   var2 = ((t_fine/2 - 64000)^2 * dig_p3 / 2^8
 *        + (t_fine/2 - 64000) * dig_p2) * 2^12
 *   var1 = (2^47 + var2) * dig_p1 / var1(0 时出错)
 *   p    = (1048576 - adc_press - var1/2^13) * 3125 / var1
 *   var1 = dig_p9 * (p/2^13)^2 / 2^25
 *   var2 = dig_p8 * p / 2^19
 *   p    = (p + var1 + var2) / 256 + dig_p7 * 16
 *
 * 输出：
 *   pressure_q24_8 — 气压 Q24.8 定点值（整数部分单位 Pa，如 9881600 → 988.16 hPa）
 * 返回 false 表示输入非法、除零或运算溢出。
 */
static inline bool dawn_bmp280_compensate_press(
	const struct dawn_bmp280_calib *calib, dawn_bmp280_s32 adc_press,
	dawn_bmp280_s32 t_fine, dawn_bmp280_u32 *pressure_q24_8)
{
	dawn_bmp280_s64 var1, var2, p, term, var1_sq;

	/* 参数合法性检查 */
	if (calib == 0 || pressure_q24_8 == 0)
		return false;
	if (adc_press < 0 || adc_press > 1048575)
		return false;

	/* var1 = (t_fine/2 - 64000)^2 * dig_p6 / 2^40 + (t_fine/2 - 64000)*dig_p5*2 + dig_p4*2^35 */
	if (dawn_bmp280_sub_s64(t_fine, 128000, &var1) ||
	    dawn_bmp280_mul_s64(var1, var1, &var1_sq) ||
	    dawn_bmp280_mul_s64(var1_sq, calib->dig_p6, &var2) ||
	    dawn_bmp280_mul_s64(var1, calib->dig_p5, &term) ||
	    dawn_bmp280_mul_s64(term, 131072, &term) ||
	    dawn_bmp280_add_s64(var2, term, &var2) ||
	    dawn_bmp280_mul_s64(calib->dig_p4, 34359738368LL, &term) ||
	    dawn_bmp280_add_s64(var2, term, &var2))
		return false;

	/* var1 = (var1_sq*dig_p3/256 + var1*dig_p2*4096) ...（注意此处 var1 被复用） */
	if (dawn_bmp280_mul_s64(var1_sq, calib->dig_p3, &term))
		return false;
	term >>= 8;
	if (dawn_bmp280_mul_s64(var1, calib->dig_p2, &p) ||
	    dawn_bmp280_mul_s64(p, 4096, &p) ||
	    dawn_bmp280_add_s64(term, p, &var1))
		return false;

	/* var1 = (2^47 + var1) * dig_p1 >> 33 */
	if (dawn_bmp280_add_s64(140737488355328LL, var1, &term) ||
	    dawn_bmp280_mul_s64(term, calib->dig_p1, &term))
		return false;
	var1 = term >> 33;

	/* 除零保护：var1 为 0 时公式无意义 */
	if (var1 == 0)
		return false;

	/* p = (1048576 - adc_press - var2/2^13... 实际为 (1048576 - adc_press) * 2^31 - var2，再 *3125 / var1 */
	if (dawn_bmp280_sub_s64(1048576, adc_press, &term) ||
	    dawn_bmp280_mul_s64(term, 2147483648LL, &term) ||
	    dawn_bmp280_sub_s64(term, var2, &term) ||
	    dawn_bmp280_mul_s64(term, 3125, &p) ||
	    dawn_bmp280_div_s64(p, var1, &p))
		return false;

	/* var1 = dig_p9 * (p>>13)^2 >> 25 */
	term = p >> 13;
	if (dawn_bmp280_mul_s64(calib->dig_p9, term, &var1) ||
	    dawn_bmp280_mul_s64(var1, term, &var1))
		return false;
	var1 >>= 25;

	/* var2 = dig_p8 * p >> 19 */
	if (dawn_bmp280_mul_s64(calib->dig_p8, p, &var2))
		return false;
	var2 >>= 19;

	/* p = (p + var1 + var2) >> 8 + dig_p7 * 16 */
	if (dawn_bmp280_add_s64(p, var1, &term) ||
	    dawn_bmp280_add_s64(term, var2, &term))
		return false;
	term >>= 8;
	if (dawn_bmp280_mul_s64(calib->dig_p7, 16, &p) ||
	    dawn_bmp280_add_s64(term, p, &p))
		return false;

	/* 结果必须为非负且能放入 32 位无符号整数 */
	if (p < 0 || p > (dawn_bmp280_s64)0xffffffff)
		return false;

	*pressure_q24_8 = (dawn_bmp280_u32)p;
	return true;
}

#endif
