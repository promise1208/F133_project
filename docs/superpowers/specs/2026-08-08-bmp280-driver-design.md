# BMP280 I2C Driver Design

## Goal

Implement a first custom Linux sensor driver for the BMP280 module and build it as a loadable kernel module. The driver is intended to teach the complete path from device-tree description and I2C register access to a user-visible sysfs interface.

The target is the F133 board's `twi1` controller, using pins PE0 and PE1. The first device-tree address assumption is `0x76`; it can be changed to `0x77` if the physical module reports that address.

## Scope

Included:

- A standalone driver under `project/driver/bmp280/`.
- An `i2c_driver` with probe and remove callbacks.
- BMP280 chip-ID validation.
- Reading and caching the factory calibration coefficients.
- Forced-mode temperature and pressure measurements.
- Datasheet compensation for temperature and pressure.
- Read-only sysfs attributes for chip ID, temperature, and pressure.
- A device-tree child node under `&twi1`.
- Cross-compilation as a `.ko` module using the existing project build rules.

Excluded from the first version:

- The kernel's existing IIO BMP280 implementation.
- IIO channels and buffered sampling.
- Interrupt-driven end-of-conversion support.
- Runtime power regulators, reset GPIOs, and suspend/resume handling.
- A user-space application or TFT display integration.
- Support for BME280 humidity or BMP180 compatibility.

## Existing Project Constraints

The project already has an out-of-tree module pattern in `driver/*`, and the top-level driver Makefile discovers subdirectories automatically. The kernel source also contains the upstream `bosch,bmp280` IIO driver, but `CONFIG_BMP280` is not enabled in the current configuration. The custom driver will use `compatible = "dawn,bmp280"` so it has an independent binding and does not conflict with the upstream driver if that option is enabled later.

The board device tree already enables `twi1`, maps it to PE0/PE1, and exposes it as I2C bus 1 through the existing aliases. The driver will use the existing 400 kHz configuration.

## Architecture

### Device tree

Add the following child node to the existing `&twi1` node:

```dts
bmp280@76 {
	compatible = "dawn,bmp280";
	reg = <0x76>;
	status = "okay";
};
```

The node does not claim a regulator or GPIO in the first version. The module must be powered correctly by the external hardware. If the module's SDO pin selects address `0x77`, only the `reg` value and node name need to change.

### I2C driver

The module will be named `dawn_bmp280`. Its probe path will:

1. Verify that the adapter supports the required I2C transactions.
2. Read register `0xD0` and require chip ID `0x58`.
3. Read the 24-byte BMP280 calibration block beginning at `0x88`.
4. Initialize the device data structure and mutex.
5. Create the sysfs attribute group.

Each measurement will be serialized by the mutex. The driver will write `CTRL_MEAS` (`0xF4`) for forced temperature/pressure measurement, wait for the conversion time, then read the six raw bytes beginning at `0xF7`. The raw values will be compensated with the integer formulas from the Bosch data sheet. The temperature fine value will be retained for pressure compensation during the same measurement.

### User-visible interface

The device will expose read-only attributes below the I2C device directory, normally:

```text
/sys/bus/i2c/devices/1-0076/chip_id
/sys/bus/i2c/devices/1-0076/temperature_mdegc
/sys/bus/i2c/devices/1-0076/pressure_pa
```

Reading `temperature_mdegc` or `pressure_pa` will trigger one fresh forced measurement. Values are reported as signed milli-degrees Celsius and integer Pascals. A failed I2C transaction or invalid conversion returns an error rather than silently returning an old value.

### Build integration

`driver/bmp280/Makefile` will include the shared `build.mk` and build `dawn_bmp280.o` as an external module. The existing `driver/Makefile` wildcard will include this directory when running `make driver`. No kernel Kconfig or kernel Makefile change is required for the first module version.

## Error Handling

- Missing I2C functionality causes probe to fail with a clear device error.
- A missing device or wrong chip ID causes probe to fail without creating sysfs attributes.
- Short or failed calibration/data reads are propagated to the caller.
- Measurement access is protected against concurrent sysfs reads.
- Device removal unregisters the sysfs group and releases managed resources.
- The driver will log probe failures and successful detection with the I2C address and chip ID.

## Verification

### Build-time checks

- Build the module in the prescribed `ubuntu18_04` container using `make -C driver/bmp280`.
- Build all external drivers using `make driver` to verify top-level discovery.
- Inspect the result with `modinfo` or `file` and confirm it is a RISC-V kernel module.

### Target checks

1. Confirm the module is physically wired to PE0/PE1 and powered at a valid voltage.
2. Confirm the address on I2C bus 1 with `i2cdetect -y 1` when `i2c-tools` is available.
3. Load the module with `insmod dawn_bmp280.ko`.
4. Confirm probe and chip-ID messages with `dmesg`.
5. Read the three sysfs attributes.
6. Compare temperature with room conditions and pressure with a reference sensor.
7. Repeat reads and confirm values update without I2C errors.

Hardware validation cannot be completed from the host filesystem alone; it requires the board, module, power, and wiring.

## Success Criteria

The first version is successful when the module builds cleanly with the project toolchain, binds to the `dawn,bmp280` node on I2C bus 1, reports chip ID `0x58`, and returns plausible temperature and pressure values through sysfs on the physical board.
