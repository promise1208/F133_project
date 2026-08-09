# F133 工程(project)

> 全志 F133 (sun20iw1p1, RISC-V 玄铁 C906) 工程代码与构建环境。

## 目录结构

```
project/
├── Makefile          # 顶层入口: make app/driver/bsp/kernel/boot/install
├── build.mk          # 共享构建配置(工具链、内核路径,自动识别容器/宿主机)
├── application/      # 用户态程序(riscv64 交叉编译,make install 装入 rootfs)
├── bsp/              # 板级支持模块 + configs/(板级配置链接)
├── driver/           # 内核驱动模块(25 个示例 + 新驱动,bmp280 等)
├── kernel/linux-5.4  # 内核源码(已配置 dawn_d1s_defconfig)
├── boot/             # 引导链: u-boot/ opensbi/ lichee/boot0/spl/ + .buildconfig
├── output/           # 编译产物镜像(烧录用)
└── bootcard/         # ★ 启动卡制作(dawn_d1s_rootfs + mkbootcard + 全部烧录工件)
```

## 板级配置(bsp/configs/)

均为指向源码内实际文件的符号链接,保证单一来源:

| 链接 | 指向 |
|---|---|
| kernel_defconfig | kernel 的 `dawn_d1s_defconfig` |
| board.dts | kernel 的 `dawn_d1s.dts` |
| uboot_defconfig | u-boot 的 `dawn_d1s_defconfig` |
| uboot_board.dts | u-boot 的 `dawn-d1s-uboot.dts` |
| spl_buildconfig | boot 的 `.buildconfig` |
| sys_config.fex | spl 的 `sys_config.fex` |

## 编译(容器内执行)

⚠️ 编译一律通过 `docker exec` 在既有容器内执行;禁止新建容器/镜像,禁止宿主机直接编译。
容器与宿主机共享工程目录(宿主机 `/media/dawn/hdd_rebuilt/share/...` ↔ 容器内 `/home/dawn/share/...`),直接编译共享路径,无副本同步。

```bash
# 容器内工程路径(共享挂载,宿主机立即可见)
P="/home/dawn/share/promise_work/F133/project"

# wangzai 工具链实际位于 opt/ext-toolchain/bin(需命令行覆盖 CROSS_PREFIX)
CROSS="/home/dawn/share/promise_work/F133/系统源码/交叉编译工具链/riscv64-wangzai-linux-gnu-gcc/opt/ext-toolchain/bin/riscv64-unknown-linux-gnu-"

# 例: 编译驱动模块(容器内)
docker exec -w "$P/driver/bmp280" ubuntu18_04 \
    make CROSS_PREFIX="$CROSS" clean all

# 例: 编译内核 Image + dtb(容器内)
docker exec -w "$P/kernel/linux-5.4" ubuntu18_04 \
    make ARCH=riscv CROSS_COMPILE="$CROSS" Image dtbs

# 例: 编译应用(容器内)
docker exec -w "$P/application" ubuntu18_04 make clean all
```

- 容器选择: 无特殊要求用 `ubuntu18_04`;SPL/OpenSBI 用 T-Head 工具链(见下)
- 顶层 `make app/driver/bsp/kernel/boot` 目标亦可,但入口有历史坑(boot/ 需 `make -C boot all` 等显式目标),按组件目录分别编译更稳
- **编译完成必须清理中间产物**(防止 Docker 存储膨胀):
  - 模块目录: `make CROSS_PREFIX=... modules_clean`(清 .o/.ko/.mod/.cmd 等)
  - 内核树: `make ARCH=riscv clean`
  - 先复制需要的产物(如 .ko → bootcard/)再清理

**产物**: 输出到 `output/` 目录:

| 文件 | 说明 |
|---|---|
| linux-Image-5.4.bin | 内核镜像 |
| dawn_d1s.dtb | 内核设备树 |
| u-boot.bin / u-boot-nodtb.bin / dawn-d1s-uboot.dtb | U-Boot |
| fw_jump.bin / fw_dynamic.bin | OpenSBI |
| boot0_{nand,sdcard,spinor}_sun20iw1p1.bin | SPL 启动固件 |
| hello_f133 | 应用示例 |
| driver_ko/*.ko | 全部驱动模块 |

烧录前将新产物更新到 `bootcard/`(boot0_sdcard.fex、fw_jump.bin、u-boot-nodtb.bin、dawn-d1s-uboot.dtb、Image、dawn_d1s.dtb、dawn_d1s_rootfs/lib/modules/5.4.61/extra/dawn_bmp280.ko)。

## 源码修改后

共享挂载实时可见,容器内编译即用最新源码,无需同步/复制。

## 烧录

启动卡工具位于本项目内 `bootcard/`:

```bash
cd "/media/dawn/hdd_rebuilt/share/promise_work/F133/project/bootcard"
./mkbootcard --check-only
sudo ./mkbootcard /dev/sdX
```

## 命名约定

板级配置统一使用 **dawn** 前缀(原厂商 wangzai 已全部改名):
`dawn_d1s_defconfig`、`dawn_d1s.dts`、`dawn-d1s-uboot.dts`、`dawn_d1s.dtb`、`dawn_d1s_rootfs`,设备树 compatible 为 `"dawn,dawn_key"` 等。
