# F133 Root Filesystem Repair Design

## Goal

Restore a bootable F133 root filesystem from the official firmware archive,
retain the current kernel, device tree, and BMP280 module integration, and
prevent a corrupt root filesystem from being written to an SD card again.

The completed repair must boot to a Linux shell and expose live BMP280 chip
ID, temperature, and pressure readings from I2C bus 1 on PE0/PE1 at address
`0x76`.

## Confirmed Root Cause

The boot chain reaches Linux, initializes `twi1`, mounts the ext4 root
filesystem, and starts `/sbin/init`. Init then exits because
`/lib64xthead/lp64d/libc.so.6` resolves to a file with an invalid ELF header.
The kernel panics because PID 1 exited.

The staged root filesystem contains multiple corrupt shared objects. Some are
zero-filled, while `libc-2.29.so` begins with unrelated host text. The same
files in `系统固件/bootcard.tar.gz` are valid RISC-V ELF objects. The kernel,
device tree, and `dawn_bmp280.ko` artifacts are valid; the module cannot be
hardware-tested because user space never starts.

## Scope

Included:

- Rebuild `烧录bootcard/dawn_d1s_rootfs` from the official archive.
- Preserve the current `Image`, `dawn_d1s.dtb`, and BMP280 module artifacts.
- Regenerate module dependency and alias metadata for kernel `5.4.61`.
- Add a non-destructive `mkbootcard --check-only` preflight mode.
- Reject missing, broken, or non-ELF critical runtime files before disk I/O.
- Test the preflight behavior before implementing it.
- Reflash the verified image when the SD card is connected to the host.
- Read back boot sectors and root filesystem files after flashing.
- Boot once and verify the BMP280 sysfs interface over the serial console.

Excluded:

- Rebuilding the kernel, bootloader, or BMP280 module when their hashes and
  metadata remain unchanged and valid.
- Changing the BMP280 compensation algorithm or device-tree address.
- Retaining the corrupt root filesystem after the replacement is verified.
- Creating or recreating build containers.

## Repair Strategy

### Fresh root filesystem

Extract `bootcard/wangzai_d1s_rootfs` from the official archive into a unique
temporary directory on healthy host storage. Do not overlay it on the corrupt
tree. Verify the official runtime loader, init binary, and shared objects before
copying the tree back to the project disk.

Install the current deployment artifacts into the fresh tree:

- `Image` at the root filesystem top level.
- `dawn_d1s.dtb` at the root filesystem top level.
- `dawn_bmp280.ko` under
  `lib/modules/5.4.61/extra/dawn_bmp280.ko`.
- Fresh `depmod` metadata for kernel `5.4.61`.

Copy the verified tree to the sibling path `dawn_d1s_rootfs.new`. Run the same
preflight against that sibling. During the final switch, rename the corrupt
tree to a temporary rollback name, rename the verified tree to
`dawn_d1s_rootfs`, re-run preflight, and then delete the temporary corrupt
tree. No corrupt backup remains after successful replacement.

### Burn-script preflight

Correct the script interpreter line and enable fail-fast shell behavior. Quote
all filesystem and device arguments used by the repair path.

Add `--check-only`, which performs all artifact and root-filesystem checks and
exits before requiring or accessing a block device. Normal flashing runs the
same preflight before partitioning or writing sectors.

The preflight verifies:

- All required boot artifacts and the root filesystem directory exist.
- `/sbin/init` and `/lib/ld-2.29.so` begin with ELF magic.
- Every regular top-level `/lib` runtime object matching `*.so*`, excluding
  Python helper scripts, begins with ELF magic.
- Top-level `/lib` symlinks are not broken.
- `dawn_bmp280.ko` exists, begins with ELF magic, and has module metadata for
  kernel `5.4.61`.
- `Image` and `dawn_d1s.dtb` exist in the burn directory.

Any failed check prints the exact file and exits nonzero before `dd`, `fdisk`,
or `mkfs.ext4` can run.

## Test Design

Create a shell regression test under `烧录bootcard/tests/`. The test uses a
unique temporary directory and a fake minimal deployment tree; it never opens
a real block device.

The test sequence is:

1. Build a minimal valid tree with ELF-magic placeholders and required files.
2. Run `mkbootcard --check-only` and require success.
3. Replace the fake libc with plain text.
4. Run `mkbootcard --check-only` and require a nonzero exit with the corrupt
   path in the error output.
5. Restore libc, create a broken library symlink, and require rejection.

The first valid-tree test must fail against the current script because
`--check-only` does not exist. Only after observing that failure will the
script be changed.

## Verification

### Host evidence

- The shell regression test passes from a clean temporary tree.
- `bash -n` accepts the burn script and regression test.
- `mkbootcard --check-only` passes on the repaired project root filesystem.
- `file`, ELF-magic checks, `modinfo`, and `depmod` confirm the runtime and
  module artifacts.
- The repaired root filesystem critical files match the official archive or
  the explicitly staged kernel, DTB, and module artifacts.
- OpenCode performs an independent read-only review.
- The container inventory remains exactly `ubuntu18_04`, `ubuntu20_04`, and
  `ubuntu22_04`.

### SD-card evidence

After fresh device identification and unmounting, flash only the confirmed
removable SD device. Read back and compare boot0, OpenSBI, early U-Boot DTB,
U-Boot, `Image`, `dawn_d1s.dtb`, and `dawn_bmp280.ko`. Leave the card unmounted
after verification.

### Physical-board evidence

Boot once at 115200 8N1 without repeated resets. The serial log must reach a
Linux shell without an init panic. Then load `dawn_bmp280` and verify:

```text
/sys/bus/i2c/devices/1-0076/chip_id
/sys/bus/i2c/devices/1-0076/temperature_mdegc
/sys/bus/i2c/devices/1-0076/pressure_pa
```

Success requires chip ID `0x58`, repeated successful reads, and plausible live
temperature and pressure values. Host checks do not substitute for this
hardware evidence.

## Failure Handling

- Do not delete the corrupt tree until the fresh sibling passes preflight.
- If extraction, copying, dependency generation, or preflight fails, leave the
  current tree in place and stop.
- If the final active-tree preflight fails, restore the temporary rollback name
  before stopping.
- Do not flash when device identity, mount state, or rootfs preflight is
  ambiguous.
- Do not create a new container to work around storage or permission failures.

## Success Criteria

The project contains a verified fresh root filesystem and a burn script that
rejects the observed corruption before disk writes. The SD card passes raw and
filesystem readback checks, Linux reaches a shell, and the physical BMP280
returns chip ID, temperature, and pressure through its sysfs attributes. The
corrupt root filesystem is not retained after successful replacement.
