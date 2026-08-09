#!/bin/bash
set -euo pipefail

TEST_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
BURN_DIR=$(cd "$TEST_DIR/.." && pwd)
TMP_DIR=$(mktemp -d '/tmp/f133-mkbootcard-test.XXXXXX')
trap 'rm -rf -- "$TMP_DIR"' EXIT

DEPLOY_DIR="$TMP_DIR/deploy"
ROOTFS="$DEPLOY_DIR/dawn_d1s_rootfs"
ROOTFS_REAL="$DEPLOY_DIR/rootfs-real"
MODULE="$ROOTFS/lib/modules/5.4.61/extra/dawn_bmp280.ko"
INIT="$ROOTFS/sbin/init"
MODULE_REAL="$ROOTFS_REAL/lib/modules/5.4.61/extra/dawn_bmp280-real.ko"
FIND_SHIM_DIR="$TMP_DIR/find-shim"
mkdir -p "$ROOTFS_REAL/bin" "$ROOTFS_REAL/sbin" \
	"$ROOTFS_REAL/lib/modules/5.4.61/extra"
ln -s "rootfs-real" "$ROOTFS"
cp "$BURN_DIR/mkbootcard" "$DEPLOY_DIR/mkbootcard"
cp "$BURN_DIR/dawn_bmp280.ko" "$DEPLOY_DIR/dawn_bmp280.ko"
cp "$BURN_DIR/dawn_bmp280.ko" \
	"$MODULE"

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

write_elf_placeholder "$ROOTFS_REAL/sbin/init"
write_elf_placeholder "$ROOTFS_REAL/lib/ld-2.29.so"
write_elf_placeholder "$ROOTFS_REAL/lib/libc-2.29.so"
ln -s "libc-2.29.so" "$ROOTFS_REAL/lib/libc.so.6"

run_preflight

write_elf_placeholder "$ROOTFS_REAL/bin/init-real"
cp "$BURN_DIR/dawn_bmp280.ko" "$MODULE_REAL"
rm "$INIT"
ln -s "/bin/init-real" "$INIT"
rm "$MODULE"
ln -s "/lib/modules/5.4.61/extra/dawn_bmp280-real.ko" "$MODULE"
if ! run_preflight >"$TMP_DIR/absolute-target.out" 2>&1; then
	cat "$TMP_DIR/absolute-target.out" >&2
	echo "FAIL: rootfs absolute target links were rejected" >&2
	exit 1
fi
rm "$INIT"
write_elf_placeholder "$ROOTFS_REAL/sbin/init"
rm "$MODULE"
cp "$BURN_DIR/dawn_bmp280.ko" "$MODULE"

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
rm "$ROOTFS/lib/libabsolute.so"

if [[ ! -e "/bin/sh" || -e "$ROOTFS_REAL/bin/sh" ]]; then
	echo "FAIL: required init escape fixture is invalid" >&2
	exit 1
fi
rm "$INIT"
ln -s "/bin/sh" "$INIT"
if run_preflight >"$TMP_DIR/init-escape.out" 2>&1; then
	echo "FAIL: required init escape passed preflight" >&2
	exit 1
fi
grep -F "$INIT" "$TMP_DIR/init-escape.out"
grep -E "escapes rootfs|missing" "$TMP_DIR/init-escape.out"
rm "$INIT"
write_elf_placeholder "$ROOTFS_REAL/sbin/init"

rm "$MODULE"
ln -s "$BURN_DIR/dawn_bmp280.ko" "$MODULE"
if run_preflight >"$TMP_DIR/module-escape.out" 2>&1; then
	echo "FAIL: deployed module escape passed preflight" >&2
	exit 1
fi
grep -F "$MODULE" "$TMP_DIR/module-escape.out"
grep -E "escapes rootfs|missing" "$TMP_DIR/module-escape.out"
rm "$MODULE"
cp "$BURN_DIR/dawn_bmp280.ko" "$MODULE"

echo "PASS: mkbootcard preflight regression tests"
