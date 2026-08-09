# F133(平地铲开发板) - 编译与烧录指南

> 供 AI 编译助手(opencode / codex)按此流程操作。工程代码统一在 `project/` 目录。

## 平台信息

- **芯片**: 全志 Allwinner F133 (sun20iw1p1),RISC-V 内核 (T-Head 玄铁 C906)
- **内核**: Linux 5.4
- **引导链**: boot0 (SPL) → OpenSBI → U-Boot → Linux
- **板级命名**: dawn(F133/project/README.md 有详细说明)

## 目录结构

```
F133/
├── project/           # ★ 工程代码与构建(见 project/README.md)
│   ├── application/ bsp/ driver/ kernel/ boot/ output/ bootcard/
├── 系统源码/          # SDK 原始材料: 工具链(已解压)、linux/u-boot/spl/opensbi 源码包
├── 驱动源码 → 已并入 project/driver
├── 烧录bootcard/      # 已并入 project/bootcard(见上)
├── 编译产物/          # 早期编译输出(旧),新产物在 project/output/
├── 芯片手册/ 软件/ 应用程序/ linux学习资料/ 平地铲Linux学习手册v1.0.pdf
└── README.md
```

## 编译环境(重要)

⚠️ **外置盘 /dev/sdb(现为 sdc)有物理坏道**,源码读取可能卡死。因此:

- **编译一律通过 `docker exec` 在既有容器内进行**(`ubuntu18_04` / `ubuntu20_04` / `ubuntu22_04`)
- **禁止创建任何新容器/镜像**(不用 docker run/create/build);容器与宿主机共享工程目录,直接编译共享路径,无副本同步
- **每次编译完成必须清理中间产物**,防止 Docker 存储膨胀
- 容器内路径: 宿主机 `/media/dawn/hdd_rebuilt/share/...` ↔ 容器内 `/home/dawn/share/...`
- 完整规则见 skill: `~/.config/opencode/skills/docker-project-builds/`

流程与命令详见 **project/README.md**(编译、清理、烧录)。

## 启动链说明(全自编译,已验证 2026-08-08)

**完整启动链已全部自编译并实测通过**(boot0 → OpenSBI → U-Boot → Linux → 登录):

| 组件 | 构建要点 |
|---|---|
| boot0_sdcard.fex | spl 编译产物**必须跑后处理**:`tools/gen_spl_fex_sd.sh`(注入 DRAM 参数/串口 PE2-PE3/校验和)。不做则 DRAM 参数全零,DDR 初始化失败,无任何输出 |
| fw_jump.bin | opensbi `make PLATFORM=thead/c910 SUNXI_CHIP=sun20iw1p1 PLATFORM_RISCV_ISA=rv64gcxthead`(CROSS_COMPILE 用 T-Head 完整路径) |
| u-boot-nodtb.bin | `dawn_d1s_defconfig` 编译;**注意 dts 的串口 compatible 与驱动必须一致**:dts 用 `dawn,uart0` 时,`drivers/serial/serial_wangzai_d1s.c` 的 of_match 也要改成 `dawn,uart0`,否则 U-Boot panic("No serial driver found") |
| dawn-d1s-uboot.dtb | u-boot 构建产物 |
| Image / dawn_d1s.dtb | 内核构建产物 |

**关键坑(全部踩过)**:
1. boot0 不做后处理 = 全零 dram_para → 完全无日志(最先遇到的坑)
2. 我们早期编译的 fw_jump 是残缺产物(57912B,应为 62008B)——**必须干净重编**,增量编译会带坏旧产物
3. u-boot dts/driver compatible 必须成对修改
4. 自己编译的 u-boot 的 bootcmd 加载 `dawn_d1s.dtb`(与官方 u-boot 的 `wangzai_d1s.dtb` 不同!),rootfs 里的 dtb 文件名要与之对应

## 工具链分工

工具链已解压于 `系统源码/交叉编译工具链/`(tar 包已清理):

| 组件 | 工具链 | 位置/要点 |
|---|---|---|
| 内核、U-Boot | `riscv64-wangzai-linux-gnu-gcc` | 解压后实际在 `riscv64-wangzai-linux-gnu-gcc/opt/ext-toolchain/bin/`,名字 riscv64-unknown-linux-gnu-gcc;编译时用命令行覆盖 `CROSS_PREFIX` 指向该 bin |
| SPL、OpenSBI | `riscv64-glibc-gcc-thead_20200702` (T-Head) | `编译spl、opensbi的专用工具链/riscv64-glibc-gcc-thead_20200702/bin/`,CROSS_COMPILE 用完整路径;SPL 用 export 环境变量 |

## 烧录方法(制作启动 TF 卡)

**不需要单独烧各 bin 文件**,用 mkbootcard 整卡制作。启动卡目录:`project/bootcard/`(含 mkbootcard、rootfs 与全部烧录工件)。

1. TF 卡插入电脑,`lsblk` 确认设备名(如 `/dev/sdc`,确认是 SD 卡不是硬盘!确认 `RM=1` 可移动)
2. 执行:
   ```bash
   cd "/media/dawn/hdd_rebuilt/share/promise_work/F133/project/bootcard"
   ./mkbootcard --check-only              # 先预检
   sudo ./mkbootcard /dev/sdX             # sdX 换成你的 TF 卡设备名
   ```
3. 卡插入开发板开机

**mkbootcard 实际做的事**:

- 预检: 校验 rootfs 结构/ELF/vermagic、boot 区无重叠、目标盘未挂载/无活动 swap/容量足够、工件暂存复验(含挂载与 swap 二次检查)
- 建一个 ext4 分区,把 `dawn_d1s_rootfs`(含 Image/dtb)烧入
- 按扇区写入引导: boot0_sdcard.fex@扇区16 → fw_jump.bin@200 → dawn-d1s-uboot.dtb@500 → u-boot-nodtb.bin@600
- `--check-only` 只预检不写盘

**文件对应关系**(project/bootcard/ 内,烧录前需用新编译产物更新):
| 文件 | 来源 |
|---|---|
| boot0_sdcard.fex | project/output/ boot0_sdcard_sun20iw1p1.bin |
| fw_jump.bin | project/output/ |
| u-boot-nodtb.bin + dawn-d1s-uboot.dtb | project/output/ |
| Image + dawn_d1s.dtb | project/output/ |
| dawn_d1s_rootfs | 官方 rootfs(系统固件/bootcard.tar.gz),已含自启脚本 `etc/init.d/S03dawn_bmp280` |

**板端 WiFi 默认配置**: `project/bootcard/dawn_d1s_rootfs/etc/wpa_supplicant.conf`(ssid/psk 占位,需按实际修改)。

## 踩坑记录(编译)

1. **工具链分工不可互换**: 内核/U-Boot → wangzai;SPL/OpenSBI → T-Head。
2. **SPL 不能独立编译**: 依赖 `.buildconfig`(boot/ 下)与 `lichee/boot0/spl` 布局。
3. **OpenSBI CROSS_COMPILE 用完整路径**;SPL 的 CROSS_COMPILE 用 export 环境变量。
4. **内核模块 Makefile include 解析**: 模块目录的 `include ../build.mk` 在 kbuild M= 模式下以内核目录为 CWD 解析,platform 子目录需用 `$(dir $(abspath $(firstword $(MAKEFILE_LIST))))../../../build.mk` 绝对路径形式。
5. **SPL 的 LICHEE_PLAT_OUT 用绝对路径**(拷贝在 nboot 子目录执行,相对路径会错位)。
6. 容器重启会丢失 /tmp/build_pdc?已删除该副本机制——现在直接编译共享挂载路径,容器重启无影响;共享盘掉线时容器挂载需 docker restart 刷新。

## 系统固件

预编译固件见 `系统固件/bootcard.tar.gz`(官方原始包,可对照烧写验证)。
