#!/bin/sh
if [ -f ../boot0_sdcard.fex ]; then
	rm ../boot0_sdcard.fex
fi

cp ../sys_config.fex ./ 
busybox unix2dos sys_config.fex 
./script sys_config.fex  
cp ../nboot/boot0_sdcard_sun20iw1p1.bin boot0_sdcard.fex 
./update_boot0 boot0_sdcard.fex sys_config.bin SDMMC_CARD  
./update_chip boot0_sdcard.fex
mv boot0_sdcard.fex ../
rm sys_config.fex sys_config.bin
#dd if=boot0_sdcard.fex of=/dev/sdb bs=512 seek=16
