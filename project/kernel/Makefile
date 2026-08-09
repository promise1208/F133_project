# ============================================================
# 内核编译 Makefile
# 用法:
#   make             编译 Image + dtb
#   make defconfig   重新生成 .config(板级 defconfig)
#   make menuconfig  图形化配置
#   make modules     编译全部模块
#   make clean
# ============================================================

include ../build.mk

.DEFAULT_GOAL := all

KSRC := linux-5.4

.PHONY: all defconfig menuconfig modules clean

all: defconfig
	$(MAKE) -C $(KSRC) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_PREFIX) -j$$(nproc) Image dtbs

defconfig:
	$(MAKE) -C $(KSRC) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_PREFIX) $(BOARD)_defconfig

menuconfig:
	$(MAKE) -C $(KSRC) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_PREFIX) menuconfig

modules:
	$(MAKE) -C $(KSRC) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_PREFIX) -j$$(nproc) modules

clean:
	$(MAKE) -C $(KSRC) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_PREFIX) clean
