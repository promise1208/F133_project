# ============================================================
# 应用程序编译模板(用户态,交叉编译)
# 用法:
#   make             编译本目录所有 .c 文件为同名可执行文件
#   make install     安装到目标 rootfs
#   make clean
#
# 新增程序: 直接放 .c 文件即可自动编译;需要额外参数用 CFLAGS
# ============================================================

include ../build.mk

.DEFAULT_GOAL := all

CC := $(CROSS_PREFIX)gcc
CFLAGS ?= -O2 -Wall -g
LDFLAGS ?=

SRCS := $(wildcard *.c)
PROGS := $(SRCS:.c=)

# 目标 rootfs 目录(烧录bootcard 里的 rootfs)
ROOTFS_DIR ?= $(SHARE_ROOT)/promise_work/F133/烧录bootcard/dawn_d1s_rootfs

.PHONY: all install clean

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

install: all
	@mkdir -p $(ROOTFS_DIR)/root
	@cp $(PROGS) $(ROOTFS_DIR)/root/
	@echo "== 已安装到 $(ROOTFS_DIR)/root/ =="

clean:
	@rm -f $(PROGS)
