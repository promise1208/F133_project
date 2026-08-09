# ============================================================
# 板级支持模块(BSP)编译模板(内核模块方式)
# 用法:
#   make             编译 bsp/ 下全部 .c 模块
#   make build/<name> 编译单个模块(如 make build/key_board)
#   make clean
#
# 板级配置见 configs/ 目录(内核/uboot defconfig、设备树等)
# ============================================================

include ../build.mk

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)

.PHONY: all clean modules

all: modules

modules:
	$(MAKE) ARCH=$(ARCH) CC=$(CC) LD=$(LD) -C $(KERNEL_DIR) M=$(CURDIR) modules

clean:
	$(MAKE) ARCH=$(ARCH) CC=$(CC) LD=$(LD) -C $(KERNEL_DIR) M=$(CURDIR) clean

# 供子目录模块使用: 每个子目录放独立 Makefile 时 include 本文件
.PHONY: mod mod_clean
mod:
	$(MAKE) ARCH=$(ARCH) CC=$(CC) LD=$(LD) -C $(KERNEL_DIR) M=$(CURDIR) modules

mod_clean:
	$(MAKE) ARCH=$(ARCH) CC=$(CC) LD=$(LD) -C $(KERNEL_DIR) M=$(CURDIR) clean
