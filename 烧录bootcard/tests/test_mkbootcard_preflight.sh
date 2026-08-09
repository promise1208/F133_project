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
cp "$BURN_DIR/dawn_bmp280.ko" "$ROOTFS/lib/modules/5.4.61/extra/dawn_bmp280.ko"
for artifact in boot0_sdcard.fex fw_jump.bin dawn-d1s-uboot.dtb u-boot-nodtb.bin Image dawn_d1s.dtb; do
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
