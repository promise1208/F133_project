#!/bin/bash
set -euo pipefail

TEST_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
BURN_DIR=$(cd "$TEST_DIR/.." && pwd)
TMP_DIR=$(mktemp -d '/tmp/f133-mkbootcard-test.XXXXXX')
trap 'rm -rf -- "$TMP_DIR"' EXIT

DEPLOY_DIR="$TMP_DIR/deploy"
ROOTFS="$DEPLOY_DIR/dawn_d1s_rootfs"
FIND_SHIM_DIR="$TMP_DIR/find-shim"
mkdir -p "$ROOTFS/sbin" "$ROOTFS/lib/modules/5.4.61/extra"
cp "$BURN_DIR/mkbootcard" "$DEPLOY_DIR/mkbootcard"
cp "$BURN_DIR/dawn_bmp280.ko" "$DEPLOY_DIR/dawn_bmp280.ko"
cp "$BURN_DIR/dawn_bmp280.ko" \
	"$ROOTFS/lib/modules/5.4.61/extra/dawn_bmp280.ko"

write_artifact_placeholder()
{
	printf 'boot artifact placeholder\n' > "$1"
}

for artifact in boot0_sdcard.fex fw_jump.bin dawn-d1s-uboot.dtb \
	u-boot-nodtb.bin Image dawn_d1s.dtb; do
	write_artifact_placeholder "$DEPLOY_DIR/$artifact"
done

write_elf_placeholder()
{
	printf '\177ELFtest-placeholder\n' > "$1"
}

run_preflight()
{
	(
		cd "$TMP_DIR"
		"$DEPLOY_DIR/mkbootcard" --check-only
	)
}

write_elf_placeholder "$ROOTFS/sbin/init"
write_elf_placeholder "$ROOTFS/lib/ld-2.29.so"
write_elf_placeholder "$ROOTFS/lib/libc-2.29.so"

run_preflight

mkdir -p "$FIND_SHIM_DIR"
printf '#!/bin/bash\nexit 42\n' > "$FIND_SHIM_DIR/find"
chmod 755 "$FIND_SHIM_DIR/find"
if (
	cd "$TMP_DIR"
	PATH="$FIND_SHIM_DIR:$PATH" "$DEPLOY_DIR/mkbootcard" --check-only
) >"$TMP_DIR/find.out" 2>&1; then
	echo "FAIL: find failure passed preflight" >&2
	exit 1
fi
grep -F "unable to scan" "$TMP_DIR/find.out"

EMPTY_ARTIFACT="$DEPLOY_DIR/boot0_sdcard.fex"
: > "$EMPTY_ARTIFACT"
if run_preflight >"$TMP_DIR/empty.out" 2>&1; then
	echo "FAIL: empty boot artifact passed preflight" >&2
	exit 1
fi
grep -F "$EMPTY_ARTIFACT" "$TMP_DIR/empty.out"
grep -F "empty" "$TMP_DIR/empty.out"
write_artifact_placeholder "$EMPTY_ARTIFACT"

printf 'not an ELF object\n' > "$ROOTFS/lib/libc-2.29.so"
if run_preflight >"$TMP_DIR/corrupt.out" 2>&1; then
	echo "FAIL: corrupt libc passed preflight" >&2
	exit 1
fi
grep -F "$ROOTFS/lib/libc-2.29.so" "$TMP_DIR/corrupt.out"
grep -F "invalid ELF magic" "$TMP_DIR/corrupt.out"

write_elf_placeholder "$ROOTFS/lib/libc-2.29.so"
ln -s missing-runtime.so "$ROOTFS/lib/libbroken.so"
if run_preflight >"$TMP_DIR/symlink.out" 2>&1; then
	echo "FAIL: broken library symlink passed preflight" >&2
	exit 1
fi
grep -F "$ROOTFS/lib/libbroken.so" "$TMP_DIR/symlink.out"
grep -F "broken symlink" "$TMP_DIR/symlink.out"

rm "$ROOTFS/lib/libbroken.so"
if [[ ! -e "/bin/sh" || -e "$ROOTFS/bin/sh" ]]; then
	echo "FAIL: absolute symlink fixture is invalid" >&2
	exit 1
fi
ln -s "/bin/sh" "$ROOTFS/lib/libabsolute.so"
if run_preflight >"$TMP_DIR/absolute-symlink.out" 2>&1; then
	echo "FAIL: absolute rootfs symlink passed preflight" >&2
	exit 1
fi
grep -F "$ROOTFS/lib/libabsolute.so" "$TMP_DIR/absolute-symlink.out"
grep -F "broken symlink" "$TMP_DIR/absolute-symlink.out"

echo "PASS: mkbootcard preflight regression tests"
