# ============================================================
# F133 工程顶层 Makefile
# 用法:
#   make app       编译应用程序(application/)
#   make driver    编译全部内核驱动模块(driver/)
#   make bsp       编译板级模块(bsp/)
#   make kernel    编译内核 Image + dtb
#   make boot      编译 U-Boot / OpenSBI / SPL(boot0)
#   make all       以上全部
#   make clean     清理
#   make install   输出镜像到 output/
# ============================================================

include build.mk

OUT_DIR := $(PROJECT_ROOT)/output

.PHONY: all app driver bsp kernel boot clean install

all: app driver bsp kernel boot

app:
	$(MAKE) -C application

driver:
	$(MAKE) -C driver

bsp:
	$(MAKE) -C bsp

kernel:
	$(MAKE) -C kernel

boot:
	$(MAKE) -C boot

clean:
	$(MAKE) -C application clean
	$(MAKE) -C driver clean
	$(MAKE) -C bsp clean

install: kernel boot
	cp kernel/linux-5.4/arch/riscv/boot/Image                    $(OUT_DIR)/linux-Image-5.4.bin
	cp kernel/linux-5.4/arch/riscv/boot/dts/sunxi/$(BOARD).dtb   $(OUT_DIR)/$(BOARD).dtb
	cp boot/u-boot/u-boot.bin                                    $(OUT_DIR)/u-boot.bin
	cp boot/u-boot/u-boot-nodtb.bin                              $(OUT_DIR)/u-boot-nodtb.bin
	cp boot/u-boot/arch/riscv/dts/dawn-d1s-uboot.dtb          $(OUT_DIR)/
	cp boot/opensbi/build/platform/thead/c910/firmware/fw_jump.bin   $(OUT_DIR)/
	cp boot/opensbi/build/platform/thead/c910/firmware/fw_dynamic.bin $(OUT_DIR)/
	cp boot/out/boot0_*_$(CHIP).bin                              $(OUT_DIR)/
	@echo "== 镜像已更新到 $(OUT_DIR) =="
