# ============================================================
# 驱动模块统一编译入口
# 用法:
#   make             编译 driver/ 下全部模块
#   make build/<name> 编译单个模块(如 make build/bitmap)
#   make clean
# ============================================================

.DEFAULT_GOAL := all
SUBDIRS := $(wildcard */)

.PHONY: all clean $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ modules

clean:
	@for d in $(SUBDIRS); do \
		$(MAKE) -C $$d modules_clean; \
	done
