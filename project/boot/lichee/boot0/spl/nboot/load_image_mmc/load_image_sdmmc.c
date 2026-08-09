// * SPDX-License-Identifier:	GPL-2.0+

#include <common.h>
#include <spare_head.h>
#include <private_boot0.h>
#include <private_uboot.h>
#include <private_toc.h>
#include <mmc_boot0.h>

int mmc_config_addr;

enum {
	E_SDMMC_OK = 0,
	E_SDMMC_NUM_ERR = 1,
	E_SDMMC_INIT_ERR = 2,
	E_SDMMC_READ_ERR = 3,
	E_SDMMC_FIND_BOOT1_ERR =4,
};


typedef struct _boot_sdcard_info_t
{
	__s32	card_ctrl_num;                //总共的卡的个数
	__s32	boot_offset;                  //指定卡启动之后，逻辑和物理分区的管理
	__s32	card_no[4];                   //当前启动的卡号, 16-31:GPIO编号，0-15:实际卡控制器编号
	__s32	speed_mode[4];                //卡的速度模式，0：低速，其它：高速
	__s32	line_sel[4];                  //卡的线制，0: 1线，其它，4线
	__s32	line_count[4];                //卡使用线的个数
}
boot_sdcard_info_t;

//card num: 0-sd 1-card3 2-emmc
int get_card_num(void)
{
	int card_num = 0;

	card_num = BT0_head.boot_head.platform[0] & 0xf;
	card_num = (card_num == 1)? 3: card_num;
	return card_num;
}

void update_flash_para(phys_addr_t uboot_base)
{
	int card_num;
	struct spare_boot_head_t  *bfh = (struct spare_boot_head_t *) uboot_base;

	card_num = get_card_num();
	set_mmc_para(card_num, (void *)&BT0_head.prvt_head.storage_data, uboot_base);
#ifdef CONFIG_ARCH_SUN50IW11
	int card_type = get_card_type();
	if (card_num == 0) {
		if (card_type == CARD_TYPE_MMC)
			bfh->boot_data.storage_type = STORAGE_EMMC0;
		else
			bfh->boot_data.storage_type = STORAGE_SD;
	}
#else
	if (card_num == 0) {
		bfh->boot_data.storage_type = STORAGE_SD;
	}
#endif
	else if (card_num == 2) {
		bfh->boot_data.storage_type = STORAGE_EMMC;
	} else if (card_num == 3) {
		bfh->boot_data.storage_type = STORAGE_EMMC3;
	}
}


int load_toc1_from_sdmmc(l_sd_f *buf, int num)
{
	int card_no, ret = 0, i, error_num = E_SDMMC_OK;
	int line_sel = 4;
	l_sd_f *sd;

	card_no = get_card_num();
	printf("card no is %d\n", card_no);
	if(card_no < 0)
	{
		error_num = E_SDMMC_NUM_ERR;
		goto __ERROR_EXIT;
	}

	printf("sdcard %d line count %d\n", card_no, line_sel );

	if( sunxi_mmc_init(card_no, line_sel, BT0_head.prvt_head.storage_gpio, 16) == -1) 
	{
		error_num = E_SDMMC_INIT_ERR;
		goto __ERROR_EXIT;;
	}

	for(i = 0; i < num; i++)
	{
		sd = &buf[i];
		printf("start_sector = %d, sector_num = %d, buf = %lx\n",
		       sd->start_sector, sd->sector_num, sd->buf);
		ret = mmc_bread(card_no, sd->start_sector, sd->sector_num, sd->buf);
		if(!ret)
		{
			error_num = E_SDMMC_READ_ERR;
			goto __ERROR_EXIT;
		}

	}
	printf("Loading boot-pkg Succeed(index=%d).\n",
		(BT0_head.boot_head.platform[0] & 0xf0)>>4);
	sunxi_mmc_exit( card_no, BT0_head.prvt_head.storage_gpio, 16 );
	return 0;

__ERROR_EXIT:
	printf("Loading boot-pkg fail(error=%d)\n",error_num);
	sunxi_mmc_exit(card_no, BT0_head.prvt_head.storage_gpio, 16 );
	return -1;

}


int load_package(void *f_struct, int f_num)
{
	return load_toc1_from_sdmmc((l_sd_f *)f_struct, f_num);
}
