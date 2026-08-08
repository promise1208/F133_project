#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "bmp280_math.h"

int main(void)
{
	struct dawn_bmp280_calib calib = {
		.dig_t1 = 27504,
		.dig_t2 = 26435,
		.dig_t3 = -1000,
		.dig_p1 = 36477,
		.dig_p2 = -10685,
		.dig_p3 = 3024,
		.dig_p4 = 2855,
		.dig_p5 = 140,
		.dig_p6 = -7,
		.dig_p7 = 15500,
		.dig_p8 = -14600,
		.dig_p9 = 6000,
	};
	int32_t t_fine = 0;
	uint32_t pressure_q24_8 = 0;

	assert(dawn_bmp280_compensate_temp(&calib, 519888, &t_fine) == 2508);
	assert(t_fine == 128422);
	assert(dawn_bmp280_compensate_press(&calib, 415148, t_fine,
						   &pressure_q24_8));
	assert(pressure_q24_8 == 25767233);
	assert((pressure_q24_8 + 128U) / 256U == 100653U);

	calib.dig_p1 = 0;
	assert(!dawn_bmp280_compensate_press(&calib, 415148, t_fine,
						    &pressure_q24_8));

	calib.dig_p1 = 36477;
	assert(!dawn_bmp280_compensate_press(NULL, 415148, t_fine,
						    &pressure_q24_8));
	assert(!dawn_bmp280_compensate_press(&calib, 415148, t_fine, NULL));
	assert(dawn_bmp280_compensate_temp(NULL, 519888, &t_fine) == 0);
	assert(dawn_bmp280_compensate_temp(&calib, 519888, NULL) == 0);

	puts("BMP280 math tests passed");
	return 0;
}
