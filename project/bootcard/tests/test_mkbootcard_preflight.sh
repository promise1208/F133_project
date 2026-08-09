#!/bin/bash
set -euo pipefail

TEST_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
BURN_DIR=$(cd "$TEST_DIR/.." && pwd)
TMP_DIR=$(mktemp -d '/tmp/f133-mkbootcard-test.XXXXXX')
ORIGINAL_PATH=$PATH
trap 'rm -rf -- "$TMP_DIR"' EXIT

DEPLOY_DIR="$TMP_DIR/deploy"
ROOTFS="$DEPLOY_DIR/dawn_d1s_rootfs"
ROOTFS_REAL="$DEPLOY_DIR/rootfs-real"
FIXTURE_ROOTFS_REAL="$ROOTFS_REAL"
MODULE="$ROOTFS/lib/modules/5.4.61/extra/dawn_bmp280.ko"
INIT="$ROOTFS/sbin/init"
MODULE_REAL="$ROOTFS_REAL/lib/modules/5.4.61/extra/dawn_bmp280-real.ko"
FIND_SHIM_DIR="$TMP_DIR/find-shim"
FLASH_PATH_DIR="$TMP_DIR/flash-path"
DEVICE_SHIM_DIR="$TMP_DIR/device-shim"
STAT_SHIM_DIR="$TMP_DIR/stat-shim"
DU_SHIM_DIR="$TMP_DIR/du-shim"
SWAPS_FILE="$TMP_DIR/swaps"
ROOTFS_BYTES=
KERNEL_BYTES=
DTB_BYTES=
REQUIRED_DEVICE_BYTES=
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

mkdir -p "$FLASH_PATH_DIR"
for command_name in dirname find mktemp modinfo od readlink rm stat tr \
	cp dd du lsblk mkfs.ext4 sync udevadm; do
	if ! command_path="$(PATH="$ORIGINAL_PATH" command -v \
		"$command_name")"; then
		echo "FAIL: missing test command $command_name" >&2
		exit 1
	fi
	ln -s "$command_path" "$FLASH_PATH_DIR/$command_name"
done
if ! (
	cd "$TMP_DIR"
	PATH="$FLASH_PATH_DIR" "$DEPLOY_DIR/mkbootcard" --check-only
) >"$TMP_DIR/check-only-tools.out" 2>&1; then
	cat "$TMP_DIR/check-only-tools.out" >&2
	echo "FAIL: check-only required flash tools" >&2
	exit 1
fi
if (
	cd "$TMP_DIR"
	PATH="$FLASH_PATH_DIR" "$DEPLOY_DIR/mkbootcard" /dev/null
) >"$TMP_DIR/flash-tools.out" 2>&1; then
	echo "FAIL: normal mode passed without fdisk" >&2
	exit 1
fi
if ! grep -F "fdisk: missing command" "$TMP_DIR/flash-tools.out"; then
	cat "$TMP_DIR/flash-tools.out" >&2
	echo "FAIL: missing fdisk was not rejected before target validation" >&2
	exit 1
fi
if [[ -e "$ROOTFS/Image" || -e "$ROOTFS/dawn_d1s.dtb" ]]; then
	echo "FAIL: normal mode copied rootfs boot files" >&2
	exit 1
fi

dd if=/dev/zero of="$DEPLOY_DIR/u-boot-nodtb.bin" bs=1M count=4 \
	status=none
if run_preflight >"$TMP_DIR/boot-region.out" 2>&1; then
	echo "FAIL: oversized boot artifact passed preflight" >&2
	exit 1
fi
grep -F "$DEPLOY_DIR/u-boot-nodtb.bin" "$TMP_DIR/boot-region.out"
grep -F "overlaps rootfs partition" "$TMP_DIR/boot-region.out"
write_artifact_placeholder "$DEPLOY_DIR/u-boot-nodtb.bin"

dd if=/dev/zero of="$DEPLOY_DIR/boot0_sdcard.fex" bs=512 count=185 \
	status=none
if run_preflight >"$TMP_DIR/boot-overlap.out" 2>&1; then
	echo "FAIL: overlapping boot artifacts passed preflight" >&2
	exit 1
fi
grep -F "$DEPLOY_DIR/boot0_sdcard.fex" "$TMP_DIR/boot-overlap.out"
grep -F "overlaps OpenSBI region" "$TMP_DIR/boot-overlap.out"
write_artifact_placeholder "$DEPLOY_DIR/boot0_sdcard.fex"

mkdir -p "$DEVICE_SHIM_DIR"
printf '%s\n' '#!/bin/bash' 'case "$*" in' \
	'"-nrpo MOUNTPOINT -- /dev/fake")' \
	'printf "%s\\n" "${TEST_MOUNTPOINT:-}" ;;' \
	'"-bdnro SIZE -- /dev/fake")' \
	'printf "%s\\n" "${TEST_DEVICE_SIZE:-1048576}" ;;' \
	'"-nrpo NAME -- /dev/fake")' \
	'printf "/dev/fake\\n/dev/fake1\\n" ;;' \
	'*) exit 64 ;;' 'esac' > "$DEVICE_SHIM_DIR/lsblk"
chmod 755 "$DEVICE_SHIM_DIR/lsblk"
mkdir -p "$STAT_SHIM_DIR"
printf '%s\n' '#!/bin/bash' \
	'printf "%s\\n" "${TEST_STAT_SIZE:-9223372036854775807}"' \
	> "$STAT_SHIM_DIR/stat"
chmod 755 "$STAT_SHIM_DIR/stat"
if (
	PATH="$STAT_SHIM_DIR:$ORIGINAL_PATH"
	export MKBOOTCARD_TEST_SOURCE=1
	source "$DEPLOY_DIR/mkbootcard"
	require_boot_region /dev/fake-artifact 600 8192 \
		"rootfs partition"
) >"$TMP_DIR/boot-overflow.out" 2>&1; then
	echo "FAIL: overflowing boot artifact size passed validation" >&2
	exit 1
fi
grep -F "/dev/fake-artifact" "$TMP_DIR/boot-overflow.out"
grep -F "overlaps rootfs partition" "$TMP_DIR/boot-overflow.out"
if (
	PATH="$STAT_SHIM_DIR:$ORIGINAL_PATH"
	export TEST_STAT_SIZE=0
	export MKBOOTCARD_TEST_SOURCE=1
	source "$DEPLOY_DIR/mkbootcard"
	require_boot_region /dev/fake-artifact 600 8192 \
		"rootfs partition"
) >"$TMP_DIR/empty-staged-artifact.out" 2>&1; then
	echo "FAIL: empty staged artifact passed validation" >&2
	exit 1
fi
grep -F "/dev/fake-artifact" "$TMP_DIR/empty-staged-artifact.out"
grep -F "empty" "$TMP_DIR/empty-staged-artifact.out"
if (
	PATH="$DEVICE_SHIM_DIR:$ORIGINAL_PATH"
	export TEST_MOUNTPOINT=/media/card
	export MKBOOTCARD_TEST_SOURCE=1
	source "$DEPLOY_DIR/mkbootcard"
	require_unmounted_device /dev/fake
) >"$TMP_DIR/mounted-device.out" 2>&1; then
	echo "FAIL: mounted target passed validation" >&2
	exit 1
fi
grep -F "/dev/fake" "$TMP_DIR/mounted-device.out"
grep -F "has mounted filesystems" "$TMP_DIR/mounted-device.out"

printf '%s\n' 'Filename Type Size Used Priority' \
	'/dev/fake1 partition 1024 0 -2' > "$SWAPS_FILE"
if (
	PATH="$DEVICE_SHIM_DIR:$ORIGINAL_PATH"
	export MKBOOTCARD_TEST_SOURCE=1
	source "$DEPLOY_DIR/mkbootcard"
	require_no_active_swap /dev/fake "$SWAPS_FILE"
) >"$TMP_DIR/active-swap.out" 2>&1; then
	echo "FAIL: active swap target passed validation" >&2
	exit 1
fi
grep -F "/dev/fake" "$TMP_DIR/active-swap.out"
grep -F "active swap" "$TMP_DIR/active-swap.out"

READLINK_SHIM_DIR="$TMP_DIR/readlink-shim"
READLINK_REAL=$(PATH="$ORIGINAL_PATH" command -v readlink)
mkdir -p "$READLINK_SHIM_DIR"
printf '%s\n' '#!/bin/bash' \
	'if [[ "$*" == *by-uuid* ]]; then' \
	'printf "/dev/fake1\\n"' \
	'else' \
	"exec \"$READLINK_REAL\" \"\$@\"" \
	'fi' > "$READLINK_SHIM_DIR/readlink"
chmod 755 "$READLINK_SHIM_DIR/readlink"
printf '%s\n' 'Filename Type Size Used Priority' \
	'/dev/disk/by-uuid/swap-partition partition 1024 0 -2' \
	> "$SWAPS_FILE"
if (
	PATH="$DEVICE_SHIM_DIR:$READLINK_SHIM_DIR:$ORIGINAL_PATH"
	export MKBOOTCARD_TEST_SOURCE=1
	source "$DEPLOY_DIR/mkbootcard"
	require_no_active_swap /dev/fake "$SWAPS_FILE"
) >"$TMP_DIR/swap-symlink.out" 2>&1; then
	echo "FAIL: symlink-named swap target passed validation" >&2
	exit 1
fi
grep -F "/dev/fake" "$TMP_DIR/swap-symlink.out"
grep -F "active swap" "$TMP_DIR/swap-symlink.out"

ROOTFS_BYTES=$(du -sb -- "$FIXTURE_ROOTFS_REAL")
ROOTFS_BYTES=${ROOTFS_BYTES%%[[:space:]]*}
KERNEL_BYTES=$(stat -c %s -- "$DEPLOY_DIR/Image")
DTB_BYTES=$(stat -c %s -- "$DEPLOY_DIR/dawn_d1s.dtb")
REQUIRED_DEVICE_BYTES=$((8192 * 512 + ROOTFS_BYTES + KERNEL_BYTES + \
	DTB_BYTES + 64 * 1024 * 1024))
if (
	PATH="$DEVICE_SHIM_DIR:$ORIGINAL_PATH"
	export TEST_DEVICE_SIZE=$((REQUIRED_DEVICE_BYTES - 1))
	export MKBOOTCARD_TEST_SOURCE=1
	source "$DEPLOY_DIR/mkbootcard"
	ROOTFS_REAL="$FIXTURE_ROOTFS_REAL"
	require_device_capacity /dev/fake
) >"$TMP_DIR/device-capacity.out" 2>&1; then
	echo "FAIL: undersized target passed validation" >&2
	exit 1
fi
grep -F "/dev/fake" "$TMP_DIR/device-capacity.out"
grep -F "insufficient capacity" "$TMP_DIR/device-capacity.out"

mkdir -p "$DU_SHIM_DIR"
printf '%s\n' '#!/bin/bash' \
	'printf "99999999999999999999\\t%s\\n" "$3"' \
	> "$DU_SHIM_DIR/du"
chmod 755 "$DU_SHIM_DIR/du"
if (
	PATH="$DEVICE_SHIM_DIR:$DU_SHIM_DIR:$ORIGINAL_PATH"
	export TEST_DEVICE_SIZE=9223372036854775807
	export MKBOOTCARD_TEST_SOURCE=1
	source "$DEPLOY_DIR/mkbootcard"
	ROOTFS_REAL="$FIXTURE_ROOTFS_REAL"
	require_device_capacity /dev/fake
) >"$TMP_DIR/capacity-overflow.out" 2>&1; then
	echo "FAIL: overflowing required capacity passed validation" >&2
	exit 1
fi
grep -F "/dev/fake" "$TMP_DIR/capacity-overflow.out"
grep -F "required capacity" "$TMP_DIR/capacity-overflow.out"
grep -F "exceeds supported range" \
	"$TMP_DIR/capacity-overflow.out"

if ! (
	PATH="$DEVICE_SHIM_DIR:$ORIGINAL_PATH"
	export TEST_MOUNTPOINT=
	export TEST_DEVICE_SIZE=$REQUIRED_DEVICE_BYTES
	export MKBOOTCARD_TEST_SOURCE=1
	source "$DEPLOY_DIR/mkbootcard"
	ROOTFS_REAL="$FIXTURE_ROOTFS_REAL"
	require_unmounted_device /dev/fake
	require_no_active_swap /dev/fake /dev/null
	require_device_capacity /dev/fake
); then
	echo "FAIL: safe target was rejected" >&2
	exit 1
fi

if [[ $(grep -c 'require_unmounted_device "$SD_DEV"' \
	"$DEPLOY_DIR/mkbootcard") -ne 2 ]]; then
	echo "FAIL: normal path lacks final mounted-device recheck" >&2
	exit 1
fi
if [[ $(grep -c 'require_no_active_swap "$SD_DEV"' \
	"$DEPLOY_DIR/mkbootcard") -ne 2 ]]; then
	echo "FAIL: normal path lacks final active-swap recheck" >&2
	exit 1
fi
FINAL_MOUNT_LINE=$(grep -n 'require_unmounted_device "$SD_DEV"' \
	"$DEPLOY_DIR/mkbootcard" | tail -n 1 | cut -d: -f1)
FINAL_SWAP_LINE=$(grep -n 'require_no_active_swap "$SD_DEV"' \
	"$DEPLOY_DIR/mkbootcard" | tail -n 1 | cut -d: -f1)
FIRST_DD_LINE=$(grep -n 'dd if="/dev/zero"' "$DEPLOY_DIR/mkbootcard" | \
	cut -d: -f1)
if ((FINAL_MOUNT_LINE >= FIRST_DD_LINE || \
	FINAL_SWAP_LINE >= FIRST_DD_LINE)); then
	echo "FAIL: final in-use checks do not precede destructive dd" >&2
	exit 1
fi

ROOTFS_IMAGE="$ROOTFS/Image"
OUTSIDE_IMAGE="$TMP_DIR/outside-image"
printf 'harmless destination\n' > "$OUTSIDE_IMAGE"
ln -s "$OUTSIDE_IMAGE" "$ROOTFS_IMAGE"
if run_preflight >"$TMP_DIR/image-destination.out" 2>&1; then
	echo "FAIL: rootfs Image destination symlink passed preflight" >&2
	exit 1
fi
grep -F "$ROOTFS_IMAGE" "$TMP_DIR/image-destination.out"
grep -F "destination must not be symlink" \
	"$TMP_DIR/image-destination.out"
rm "$ROOTFS_IMAGE"

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
