# ============================================================
# 引导链编译 Makefile: U-Boot / OpenSBI / SPL(boot0)
# 用法:
#   make              全部编译(u-boot + opensbi + spl)
#   make uboot        仅 U-Boot(wangzai 工具链)
#   make opensbi      仅 OpenSBI(T-Head 工具链,需完整路径)
#   make spl          仅 SPL boot0(T-Head 工具链)
#   make clean
# 说明: spl 布局 lichee/boot0/spl + ../../../.buildconfig(已配置)
#       spl 输出到 boot/out/,opensbi 输出到 opensbi/build/
# ============================================================

include ../build.mk

.PHONY: all uboot opensbi spl clean

all: uboot opensbi spl

uboot:
	$(MAKE) -C u-boot ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_PREFIX) $(BOARD)_defconfig
	$(MAKE) -C u-boot ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_PREFIX) -j$$(nproc)

opensbi:
	$(MAKE) -C opensbi CROSS_COMPILE=$(THEAD_PREFIX) PLATFORM=thead/c910 SUNXI_CHIP=$(CHIP) PLATFORM_RISCV_ISA=rv64gcxthead

spl:
	@export CROSS_COMPILE="$(THEAD_PREFIX)"; \
	$(MAKE) -C lichee/boot0/spl p=sun20iw1p1 m=spinor boot0

clean:
	$(MAKE) -C u-boot ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_PREFIX) clean || true
	$(MAKE) -C opensbi CROSS_COMPILE=$(THEAD_PREFIX) PLATFORM=thead/c910 SUNXI_CHIP=$(CHIP) PLATFORM_RISCV_ISA=rv64gcxthead clean || true
	@export CROSS_COMPILE="$(THEAD_PREFIX)"; \
	$(MAKE) -C lichee/boot0/spl p=sun20iw1p1 clean || true
