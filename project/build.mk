# ============================================================
# F133 工程共享构建配置
# 所有子目录 Makefile 通过 include 本文件获得统一的环境变量
# 容器内(/home/dawn/share)与宿主机(/media/dawn/hdd_rebuilt/share)自动识别
# 可通过命令行覆盖: make SDK_ROOT=/xxx CROSS_PREFIX=/xxx/riscv64-unknown-linux-gnu- ...
# ============================================================

# ---- 路径自动识别 ----
ifeq ($(wildcard /home/dawn/share),)
SHARE_ROOT := /media/dawn/hdd_rebuilt/share
else
SHARE_ROOT := /home/dawn/share
endif

SDK_ROOT ?= $(SHARE_ROOT)/promise_work/F133/系统源码
PROJECT_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

# ---- 工具链 ----
# 内核/U-Boot 用 wangzai 工具链
CROSS_PREFIX ?= $(SDK_ROOT)/交叉编译工具链/riscv64-wangzai-linux-gnu-gcc/bin/riscv64-unknown-linux-gnu-
# SPL/OpenSBI 用 T-Head 专用工具链
THEAD_PREFIX ?= $(SDK_ROOT)/交叉编译工具链/编译spl、opensbi的专用工具链/riscv64-glibc-gcc-thead_20200702/bin/riscv64-unknown-linux-gnu-

# ---- 内核源码树(编译模块用,已 make modules_prepare)----
KERNEL_DIR ?= $(PROJECT_ROOT)/kernel/linux-5.4
ARCH ?= riscv

CC := $(CROSS_PREFIX)gcc
LD := $(CROSS_PREFIX)ld

# ---- 目标板 ----
BOARD ?= dawn_d1s
CHIP ?= sun20iw1p1

# ---- 内核模块通用编译规则 ----
# 用法: 在模块目录 include $(PROJECT_ROOT)/build.mk,定义 obj-m 即可
.PHONY: modules modules_clean
modules:
	$(MAKE) ARCH=$(ARCH) CC=$(CC) LD=$(LD) -C $(KERNEL_DIR) M=$(CURDIR) modules

modules_clean:
	$(MAKE) ARCH=$(ARCH) CC=$(CC) LD=$(LD) -C $(KERNEL_DIR) M=$(CURDIR) clean
