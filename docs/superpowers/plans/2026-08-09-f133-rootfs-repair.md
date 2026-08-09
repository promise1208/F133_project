# F133 Root Filesystem Repair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the corrupt staged F133 root filesystem with a verified official tree, prevent future corrupt-rootfs burns, reflash the SD card, and verify live BMP280 readings.

**Architecture:** Add a side-effect-free preflight path to `mkbootcard` and cover it with a temporary-tree shell regression test. Rebuild the deployment tree on healthy `/tmp` storage, install the already-verified kernel, DTB, and BMP280 module, atomically activate it only after validation, then independently review, burn, read back, and boot-test it.

**Tech Stack:** Bash, GNU coreutils, tar, kmod (`modinfo`, `depmod`), util-linux (`lsblk`, `fdisk`), e2fsprogs (`mkfs.ext4`), OpenCode, Linux 5.4.61 RISC-V kernel module/sysfs.

---

## File Map

- Create `烧录bootcard/tests/test_mkbootcard_preflight.sh`: isolated regression test for valid, corrupt-ELF, and broken-symlink preflight cases.
- Modify `烧录bootcard/mkbootcard`: shared preflight, `--check-only`, fail-fast behavior, quoted paths, and delayed block-device access.
- Replace generated deployment directory `烧录bootcard/dawn_d1s_rootfs`: freshly extracted official rootfs plus current deployment artifacts; do not add this generated tree to Git.
- Read `系统固件/bootcard.tar.gz`: immutable official rootfs source.
- Read `烧录bootcard/Image`, `烧录bootcard/dawn_d1s.dtb`, and
  `烧录bootcard/dawn_bmp280.ko`: current verified deployment artifacts. The
  similarly named `project/output` files are older builds and must not replace
  these deployment copies.

### Task 1: Add a failing preflight regression test

**Files:**
- Create: `烧录bootcard/tests/test_mkbootcard_preflight.sh`
- Test: `烧录bootcard/tests/test_mkbootcard_preflight.sh`

- [ ] **Step 1: Create the test with a real module and fake non-destructive deployment tree**

```bash
#!/bin/bash
set -euo pipefail

TEST_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
BURN_DIR=$(cd "$TEST_DIR/.." && pwd)
TMP_DIR=$(mktemp -d /tmp/f133-mkbootcard-test.XXXXXX)
trap 'rm -rf -- "$TMP_DIR"' EXIT

DEPLOY_DIR="$TMP_DIR/deploy"
ROOTFS="$DEPLOY_DIR/dawn_d1s_rootfs"
mkdir -p "$ROOTFS/sbin" "$ROOTFS/lib/modules/5.4.61/extra"
cp "$BURN_DIR/mkbootcard" "$DEPLOY_DIR/mkbootcard"
cp "$BURN_DIR/dawn_bmp280.ko" "$DEPLOY_DIR/dawn_bmp280.ko"
cp "$BURN_DIR/dawn_bmp280.ko" \
    "$ROOTFS/lib/modules/5.4.61/extra/dawn_bmp280.ko"

for artifact in boot0_sdcard.fex fw_jump.bin dawn-d1s-uboot.dtb \
    u-boot-nodtb.bin Image dawn_d1s.dtb; do
    : > "$DEPLOY_DIR/$artifact"
done

write_elf_placeholder()
{
    printf '\177ELFtest-placeholder\n' > "$1"
}

write_elf_placeholder "$ROOTFS/sbin/init"
write_elf_placeholder "$ROOTFS/lib/ld-2.29.so"
write_elf_placeholder "$ROOTFS/lib/libc-2.29.so"

"$DEPLOY_DIR/mkbootcard" --check-only

printf 'not an ELF object\n' > "$ROOTFS/lib/libc-2.29.so"
if "$DEPLOY_DIR/mkbootcard" --check-only >"$TMP_DIR/corrupt.out" 2>&1; then
    echo "FAIL: corrupt libc passed preflight" >&2
    exit 1
fi
grep -F "$ROOTFS/lib/libc-2.29.so" "$TMP_DIR/corrupt.out"

write_elf_placeholder "$ROOTFS/lib/libc-2.29.so"
ln -s missing-runtime.so "$ROOTFS/lib/libbroken.so"
if "$DEPLOY_DIR/mkbootcard" --check-only >"$TMP_DIR/symlink.out" 2>&1; then
    echo "FAIL: broken library symlink passed preflight" >&2
    exit 1
fi
grep -F "$ROOTFS/lib/libbroken.so" "$TMP_DIR/symlink.out"

echo "PASS: mkbootcard preflight regression tests"
```

- [ ] **Step 2: Make the test executable and verify shell syntax**

Run:

```bash
chmod +x 烧录bootcard/tests/test_mkbootcard_preflight.sh
bash -n 烧录bootcard/tests/test_mkbootcard_preflight.sh
```

Expected: `bash -n` exits `0` with no output.

- [ ] **Step 3: Run the test and observe the required RED failure**

Run:

```bash
烧录bootcard/tests/test_mkbootcard_preflight.sh
```

Expected: nonzero exit because the current `mkbootcard` treats `--check-only` as a device and reports it missing.

- [ ] **Step 4: Commit only the regression test**

```bash
git add 烧录bootcard/tests/test_mkbootcard_preflight.sh
git commit -m "test: cover bootcard rootfs preflight"
```

### Task 2: Implement side-effect-free burn preflight

**Files:**
- Modify: `烧录bootcard/mkbootcard`
- Test: `烧录bootcard/tests/test_mkbootcard_preflight.sh`

- [ ] **Step 1: Replace the script setup with fail-fast, location-independent paths**

```bash
#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
BOOTLOADER="$SCRIPT_DIR/boot0_sdcard.fex"
OPENSBI="$SCRIPT_DIR/fw_jump.bin"
EARLY_DTB="$SCRIPT_DIR/dawn-d1s-uboot.dtb"
UBOOT="$SCRIPT_DIR/u-boot-nodtb.bin"
KERNEL="$SCRIPT_DIR/Image"
DTB="$SCRIPT_DIR/dawn_d1s.dtb"
ROOTFS="$SCRIPT_DIR/dawn_d1s_rootfs"
MODULE="$ROOTFS/lib/modules/5.4.61/extra/dawn_bmp280.ko"
KERNEL_RELEASE=5.4.61
```

- [ ] **Step 2: Add exact file, ELF, symlink, and module-metadata checks**

```bash
die()
{
    echo "Error: $*" >&2
    exit 1
}

require_file()
{
    [ -f "$1" ] || die "$1 does not exist"
}

require_elf()
{
    require_file "$1"
    magic=$(od -An -tx1 -N4 "$1" | tr -d ' \n')
    [ "$magic" = "7f454c46" ] || die "$1 has invalid ELF magic"
}

preflight()
{
    require_file "$BOOTLOADER"
    require_file "$OPENSBI"
    require_file "$EARLY_DTB"
    require_file "$UBOOT"
    require_file "$KERNEL"
    require_file "$DTB"
    [ -d "$ROOTFS" ] || die "$ROOTFS does not exist"

    require_elf "$ROOTFS/sbin/init"
    require_elf "$ROOTFS/lib/ld-2.29.so"
    require_elf "$ROOTFS/lib/libc-2.29.so"

    while IFS= read -r -d '' runtime; do
        case "$runtime" in
            *.py) continue ;;
        esac
        require_elf "$runtime"
    done < <(find "$ROOTFS/lib" -maxdepth 1 -type f -name '*.so*' -print0)

    while IFS= read -r -d '' link; do
        [ -e "$link" ] || die "$link is a broken symlink"
    done < <(find "$ROOTFS/lib" -maxdepth 1 -type l -print0)

    require_elf "$MODULE"
    vermagic=$(modinfo -F vermagic "$MODULE")
    case "$vermagic" in
        "$KERNEL_RELEASE "*) ;;
        *) die "$MODULE has unexpected vermagic: $vermagic" ;;
    esac
    modinfo -F alias "$MODULE" | grep -Fx 'i2c:dawn_bmp280' >/dev/null ||
        die "$MODULE is missing i2c:dawn_bmp280 alias"

    echo "Preflight passed: $ROOTFS"
}
```

- [ ] **Step 3: Parse `--check-only` before any block-device access**

```bash
if [ "${1:-}" = "--check-only" ]; then
    [ "$#" -eq 1 ] || die "usage: $0 --check-only"
    preflight
    exit 0
fi

[ "$#" -eq 1 ] || die "usage: $0 --check-only | /dev/sdX"
preflight

SD_DEV=$1
[ -b "$SD_DEV" ] || die "$SD_DEV is not a block device"
REMOVABLE=$(lsblk -dn -o RM -- "$SD_DEV")
[ "$REMOVABLE" = "1" ] || die "$SD_DEV is not a removable disk"
case "$SD_DEV" in
    *[0-9]) ROOTFS_PART="${SD_DEV}p1" ;;
    *) ROOTFS_PART="${SD_DEV}1" ;;
esac
```

- [ ] **Step 4: Keep the existing burn layout while quoting every path**

```bash
cp "$KERNEL" "$ROOTFS/Image"
cp "$DTB" "$ROOTFS/dawn_d1s.dtb"

echo "Creating image file $SD_DEV..."
dd if=/dev/zero of="$SD_DEV" bs=1M count=4
sync
echo "Creating partitions..."
(
echo o
echo n
echo p
echo 1
echo 8192
echo
echo w
) | fdisk "$SD_DEV"
sync

echo "Creating rootfs on $ROOTFS_PART"
mkfs.ext4 "$ROOTFS_PART" -L rootfs -d "$ROOTFS"
sync

echo "Writing bootloader: $BOOTLOADER"
dd if="$BOOTLOADER" of="$SD_DEV" seek=16 bs=512
echo "Writing OpenSBI: $OPENSBI"
dd if="$OPENSBI" of="$SD_DEV" seek=200 bs=512
echo "Writing early DTB: $EARLY_DTB"
dd if="$EARLY_DTB" of="$SD_DEV" seek=500 bs=512
echo "Writing U-Boot: $UBOOT"
dd if="$UBOOT" of="$SD_DEV" seek=600 bs=512
sync
```

- [ ] **Step 5: Run syntax and regression checks to reach GREEN**

Run:

```bash
bash -n 烧录bootcard/mkbootcard
bash -n 烧录bootcard/tests/test_mkbootcard_preflight.sh
烧录bootcard/tests/test_mkbootcard_preflight.sh
```

Expected: both syntax checks exit `0`; the test ends with `PASS: mkbootcard preflight regression tests`. No block device is opened.

- [ ] **Step 6: Commit the implementation**

```bash
git add 烧录bootcard/mkbootcard
git commit -m "fix: reject corrupt bootcard rootfs"
```

### Task 3: Rebuild and activate a fresh deployment rootfs

**Files:**
- Replace: `烧录bootcard/dawn_d1s_rootfs`
- Read: `系统固件/bootcard.tar.gz`
- Read: `烧录bootcard/Image`
- Read: `烧录bootcard/dawn_d1s.dtb`
- Read: `烧录bootcard/dawn_bmp280.ko`

- [ ] **Step 1: Record immutable source and deployment hashes**

Run:

```bash
sha256sum 系统固件/bootcard.tar.gz \
    烧录bootcard/Image \
    烧录bootcard/dawn_d1s.dtb \
    烧录bootcard/dawn_bmp280.ko
```

Expected: four hashes are recorded in the execution log. Image remains
`7d175e01652152ba48a504365a08fa5f213b03a77e96a2fa33af9bf35964f9c4`,
DTB remains
`356b63a516bdf5aa0775a7c3a47c00ab3b54b8e90088cdec412a350794c90f80`,
and the module remains
`4ac6f5d4787ef028b57705487b7b98a54a5747b5b3815b284f477000dd334a0d`.

- [ ] **Step 2: Extract the official tree onto healthy temporary storage**

Run:

```bash
REPAIR_TMP=$(mktemp -d /tmp/f133-rootfs-repair.XXXXXX)
tar -xzf 系统固件/bootcard.tar.gz -C "$REPAIR_TMP" \
    bootcard/wangzai_d1s_rootfs
FRESH_ROOTFS="$REPAIR_TMP/bootcard/wangzai_d1s_rootfs"
file "$FRESH_ROOTFS/sbin/init" "$FRESH_ROOTFS/lib/ld-2.29.so" \
    "$FRESH_ROOTFS/lib/libc-2.29.so"
```

Expected: all three files are identified as ELF 64-bit RISC-V objects.

- [ ] **Step 3: Install the current kernel, DTB, and BMP280 module**

Run:

```bash
install -m 0644 烧录bootcard/Image "$FRESH_ROOTFS/Image"
install -m 0644 烧录bootcard/dawn_d1s.dtb "$FRESH_ROOTFS/dawn_d1s.dtb"
install -d "$FRESH_ROOTFS/lib/modules/5.4.61/extra"
install -m 0644 烧录bootcard/dawn_bmp280.ko \
    "$FRESH_ROOTFS/lib/modules/5.4.61/extra/dawn_bmp280.ko"
depmod -b "$FRESH_ROOTFS" 5.4.61
```

Expected: commands exit `0`; `modules.dep` names `extra/dawn_bmp280.ko` and `modules.alias` contains `i2c:dawn_bmp280`.

- [ ] **Step 4: Validate the fresh tree before touching the active tree**

Run:

```bash
file "$FRESH_ROOTFS/lib/modules/5.4.61/extra/dawn_bmp280.ko"
modinfo -F vermagic "$FRESH_ROOTFS/lib/modules/5.4.61/extra/dawn_bmp280.ko"
grep -F 'extra/dawn_bmp280.ko' "$FRESH_ROOTFS/lib/modules/5.4.61/modules.dep"
grep -F 'i2c:dawn_bmp280' "$FRESH_ROOTFS/lib/modules/5.4.61/modules.alias"
```

Expected: RISC-V ELF module, vermagic beginning `5.4.61 `, and both metadata searches succeed.

- [ ] **Step 5: Copy to a sibling candidate and preflight it without changing the active tree**

Build an isolated preflight view whose `dawn_d1s_rootfs` symlink resolves to
the sibling candidate, then run the unchanged deployment script from that
view:

```bash
if [ -e 烧录bootcard/dawn_d1s_rootfs.new ]; then
    echo "Refusing to overwrite existing dawn_d1s_rootfs.new" >&2
    exit 1
fi
mkdir 烧录bootcard/dawn_d1s_rootfs.new
cp -a "$FRESH_ROOTFS/." 烧录bootcard/dawn_d1s_rootfs.new/
CANDIDATE_CHECK=$(mktemp -d /tmp/f133-rootfs-check.XXXXXX)
cp 烧录bootcard/mkbootcard "$CANDIDATE_CHECK/mkbootcard"
for artifact in boot0_sdcard.fex fw_jump.bin dawn-d1s-uboot.dtb \
    u-boot-nodtb.bin Image dawn_d1s.dtb; do
    ln -s "$PWD/烧录bootcard/$artifact" "$CANDIDATE_CHECK/$artifact"
done
ln -s "$PWD/烧录bootcard/dawn_d1s_rootfs.new" \
    "$CANDIDATE_CHECK/dawn_d1s_rootfs"
"$CANDIDATE_CHECK/mkbootcard" --check-only
rm -rf -- "$CANDIDATE_CHECK"
```

Expected: preflight passes while the corrupt active tree remains untouched. If it fails, stop and leave both the active tree and fresh sibling in place for diagnosis.

- [ ] **Step 6: Permanently remove the corrupt switched-out tree only after active-tree verification**

Run:

```bash
mv 烧录bootcard/dawn_d1s_rootfs \
    烧录bootcard/dawn_d1s_rootfs.corrupt-switch
mv 烧录bootcard/dawn_d1s_rootfs.new 烧录bootcard/dawn_d1s_rootfs
if ! 烧录bootcard/mkbootcard --check-only; then
    mv 烧录bootcard/dawn_d1s_rootfs 烧录bootcard/dawn_d1s_rootfs.new
    mv 烧录bootcard/dawn_d1s_rootfs.corrupt-switch \
        烧录bootcard/dawn_d1s_rootfs
    exit 1
fi
rm -rf -- 烧录bootcard/dawn_d1s_rootfs.corrupt-switch
rm -rf -- "$REPAIR_TMP"
test ! -e 烧录bootcard/dawn_d1s_rootfs.corrupt-switch
```

Expected: final preflight succeeds and the corrupt tree no longer exists. This deletion is intentional and cannot be recovered except by rebuilding from the official archive.

### Task 4: Perform host verification and independent review

**Files:**
- Verify: `烧录bootcard/mkbootcard`
- Verify: `烧录bootcard/tests/test_mkbootcard_preflight.sh`
- Verify: `烧录bootcard/dawn_d1s_rootfs`

- [ ] **Step 1: Run all local regression and artifact checks from scratch**

Run:

```bash
bash -n 烧录bootcard/mkbootcard
bash -n 烧录bootcard/tests/test_mkbootcard_preflight.sh
烧录bootcard/tests/test_mkbootcard_preflight.sh
烧录bootcard/mkbootcard --check-only
file 烧录bootcard/dawn_d1s_rootfs/sbin/init \
    烧录bootcard/dawn_d1s_rootfs/lib/ld-2.29.so \
    烧录bootcard/dawn_d1s_rootfs/lib/libc-2.29.so \
    烧录bootcard/dawn_d1s_rootfs/lib/modules/5.4.61/extra/dawn_bmp280.ko
```

Expected: all commands exit `0`; runtime and module files are RISC-V ELF objects.

- [ ] **Step 2: Confirm the restricted container inventory is unchanged**

Run:

```bash
docker ps -a --format '{{.Names}}' | sort
```

Expected output is exactly:

```text
ubuntu18_04
ubuntu20_04
ubuntu22_04
```

- [ ] **Step 3: Ask OpenCode for a read-only review using the required model**

Run from the repository root:

```bash
opencode run --model deepseek/deepseek-v4-flash --variant max \
    "Read only. Review 烧录bootcard/mkbootcard, 烧录bootcard/tests/test_mkbootcard_preflight.sh, and docs/superpowers/specs/2026-08-09-f133-rootfs-repair-design.md. Focus on destructive-device safety, preflight bypasses, shell quoting, ELF/module validation, and missing tests. Do not edit files. Return findings ordered by severity with file and line references."
```

Expected: a completed findings report and no worktree changes from OpenCode.

- [ ] **Step 4: Resolve valid review findings with RED/GREEN tests and rerun Task 4 Steps 1-3**

For each valid behavioral finding, first add a reproducing case to `test_mkbootcard_preflight.sh`, run it to observe failure, minimally fix `mkbootcard`, and rerun the complete regression test. Reject suggestions that expand beyond the approved repair design.

- [ ] **Step 5: Commit review-driven corrections if any**

```bash
git add 烧录bootcard/mkbootcard 烧录bootcard/tests/test_mkbootcard_preflight.sh
git diff --cached --check
git commit -m "fix: address bootcard preflight review"
```

Expected: commit only when there are actual staged corrections; otherwise skip this step.

### Task 5: Identify, flash, and read back the SD card

**Files:**
- Read from: `烧录bootcard/`
- Destructively write only to: the freshly identified removable SD block device

- [ ] **Step 1: Require a freshly connected removable device before choosing any target**

Run:

```bash
lsblk -p -o NAME,SIZE,TYPE,RM,TRAN,MOUNTPOINT
```

Expected: one user-confirmed whole-disk device has `TYPE=disk`, `RM=1`, expected SD capacity/transport, and is not a system disk. If no such device exists, stop and request that the SD card be inserted into the host.

- [ ] **Step 2: Resolve and validate the exact target interactively, without globs**

After the confirmed path is visible, assign that literal path to `F133_SD_DEV`, then run:

```bash
test -b "$F133_SD_DEV"
test "$(lsblk -dn -o TYPE -- "$F133_SD_DEV")" = disk
test "$(lsblk -dn -o RM -- "$F133_SD_DEV")" = 1
lsblk -p -o NAME,SIZE,TYPE,RM,TRAN,MOUNTPOINT -- "$F133_SD_DEV"
```

Expected: every guard succeeds and the printed device identity matches Step 1. Do not continue on ambiguity.

- [ ] **Step 3: Unmount only partitions belonging to the confirmed SD and burn it**

Enumerate only child partitions of the confirmed disk and unmount a child only
when `findmnt` proves that exact child is mounted. Then rerun the guards from
Step 2 and execute:

```bash
while read -r child child_type; do
    if [ "$child_type" = part ] && findmnt -rn -S "$child" >/dev/null; then
        sudo umount -- "$child"
    fi
done < <(lsblk -lnpo NAME,TYPE -- "$F133_SD_DEV")
test -z "$(lsblk -lnpo MOUNTPOINT -- "$F133_SD_DEV" | sed '/^[[:space:]]*$/d')"
烧录bootcard/mkbootcard --check-only
sudo 烧录bootcard/mkbootcard "$F133_SD_DEV"
```

Expected: preflight passes before disk access; partitioning, ext4 population, and all four raw writes complete with exit `0`.

- [ ] **Step 4: Read back and compare all raw boot components**

Create a unique temporary readback directory and compare exact byte lengths:

```bash
READBACK_DIR=$(mktemp -d /tmp/f133-sd-readback.XXXXXX)
sudo dd if="$F133_SD_DEV" of="$READBACK_DIR/boot0.bin" bs=512 skip=16 \
    count=$((($(stat -c %s 烧录bootcard/boot0_sdcard.fex)+511)/512))
sudo dd if="$F133_SD_DEV" of="$READBACK_DIR/opensbi.bin" bs=512 skip=200 \
    count=$((($(stat -c %s 烧录bootcard/fw_jump.bin)+511)/512))
sudo dd if="$F133_SD_DEV" of="$READBACK_DIR/early-dtb.bin" bs=512 skip=500 \
    count=$((($(stat -c %s 烧录bootcard/dawn-d1s-uboot.dtb)+511)/512))
sudo dd if="$F133_SD_DEV" of="$READBACK_DIR/uboot.bin" bs=512 skip=600 \
    count=$((($(stat -c %s 烧录bootcard/u-boot-nodtb.bin)+511)/512))
cmp -n "$(stat -c %s 烧录bootcard/boot0_sdcard.fex)" \
    烧录bootcard/boot0_sdcard.fex "$READBACK_DIR/boot0.bin"
cmp -n "$(stat -c %s 烧录bootcard/fw_jump.bin)" \
    烧录bootcard/fw_jump.bin "$READBACK_DIR/opensbi.bin"
cmp -n "$(stat -c %s 烧录bootcard/dawn-d1s-uboot.dtb)" \
    烧录bootcard/dawn-d1s-uboot.dtb "$READBACK_DIR/early-dtb.bin"
cmp -n "$(stat -c %s 烧录bootcard/u-boot-nodtb.bin)" \
    烧录bootcard/u-boot-nodtb.bin "$READBACK_DIR/uboot.bin"
```

Expected: all four `cmp` commands exit `0`.

- [ ] **Step 5: Mount the SD read-only and compare filesystem artifacts**

After the burn, settle device events, derive the partition path without a glob,
and mount it read-only in a unique directory:

```bash
sudo udevadm settle
F133_SD_PART=$(lsblk -lnpo NAME,TYPE -- "$F133_SD_DEV" | \
    awk '$2 == "part" {print $1}')
test -n "$F133_SD_PART"
test "$(printf '%s\n' "$F133_SD_PART" | wc -l)" -eq 1
READBACK_MOUNT=$(mktemp -d /tmp/f133-sd-mount.XXXXXX)
sudo mount -o ro -- "$F133_SD_PART" "$READBACK_MOUNT"
cmp 烧录bootcard/Image "$READBACK_MOUNT/Image"
cmp 烧录bootcard/dawn_d1s.dtb "$READBACK_MOUNT/dawn_d1s.dtb"
cmp 烧录bootcard/dawn_bmp280.ko \
    "$READBACK_MOUNT/lib/modules/5.4.61/extra/dawn_bmp280.ko"
file "$READBACK_MOUNT/sbin/init" "$READBACK_MOUNT/lib/libc-2.29.so"
sudo umount -- "$READBACK_MOUNT"
rmdir "$READBACK_MOUNT"
rm -rf -- "$READBACK_DIR"
```

Expected: all comparisons exit `0`, and both runtime files are RISC-V ELF. Unmount the read-only mount, remove temporary readback directories, and leave the SD unmounted.

### Task 6: Boot once and verify BMP280 hardware

**Files:**
- Runtime interface: `/sys/bus/i2c/devices/1-0076/`
- Serial device: `/dev/ttyUSB0` at 115200 8N1

- [ ] **Step 1: Capture one clean boot without repeated resets**

Insert the verified SD into the F133 board, open `/dev/ttyUSB0` at 115200 8N1, reset once, and capture the complete log.

Expected: boot0, OpenSBI, U-Boot, and Linux complete; `/sbin/init` starts and a shell/login prompt appears without `invalid ELF header` or `Attempted to kill init`.

- [ ] **Step 2: Load the BMP280 module and inspect binding evidence**

Run on the target shell:

```bash
modprobe dawn_bmp280
dmesg | tail -n 40
test -d /sys/bus/i2c/devices/1-0076
```

Expected: module loads without unresolved symbols, the I2C device binds at bus 1 address `0x76`, and the sysfs directory exists.

- [ ] **Step 3: Read chip ID and repeat live measurements**

Run on the target shell:

```bash
cat /sys/bus/i2c/devices/1-0076/chip_id
for sample in 1 2 3 4 5; do
    cat /sys/bus/i2c/devices/1-0076/temperature_mdegc
    cat /sys/bus/i2c/devices/1-0076/pressure_pa
    sleep 1
done
```

Expected: chip ID is `0x58`; all five temperature and pressure reads succeed and return plausible live integer values rather than fixed errors.

- [ ] **Step 4: Record final evidence and repository state**

Run on the host:

```bash
git status --short
git log --oneline -5
sha256sum 烧录bootcard/Image 烧录bootcard/dawn_d1s.dtb \
    烧录bootcard/dawn_bmp280.ko
docker ps -a --format '{{.Names}}' | sort
```

Expected: only known pre-existing/untracked project content remains, repair commits are visible, deployment hashes match the verified artifacts, and exactly the three approved containers remain.
