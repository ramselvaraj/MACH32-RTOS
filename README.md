# MACH32-RTOS

A DC motor controller for the NUCLEO-F446RE.

MACH32 operates the peripherals. FreeRTOS schedules the tasks.

**Status: The firmware builds and host tests pass. Hardware tests remain pending.**

**The default build keeps the motor output disabled, including after an ARM command.**

## Scope

- Control motor power and direction through an L293D driver.
- Select potentiometer input or serial control.
- Disable the output after STOP or a serial command timeout.
- Check task progress before each watchdog refresh.
- Report operating state, faults, timing data, and stack margins.

This version does not measure motor speed or current. It does not use CAN.
Disabling the output does not stop the shaft immediately.
This project does not provide certified safety functions.

## Build and test

Use GNU Make, an Arm GNU toolchain with newlib, a host C compiler, Git, and Python 3.10 or later.

Initialize the dependencies:

```sh
make deps
```

Build the default firmware:

```sh
make -j4
```

Check the linked interrupt vectors and the absence of heap functions:

```sh
make verify
```

Run the host tests:

```sh
make test
make test-sanitize
make check-python
```

The default build produces these files:

```text
build/output0-limit25-hooks0/firmware.elf
build/output0-limit25-hooks0/firmware.bin
build/output0-limit25-hooks0/firmware.hex
build/output0-limit25-hooks0/firmware.map
```

Each build configuration has a separate directory. A configuration change cannot reuse incompatible object files.

## First board test

1. Disconnect motor power.
2. Read [the wiring guide](docs/wiring.md).
3. Check the board type and ST-LINK connection.

**WARNING: `make flash` writes at `0x08000000`. It replaces an existing bootloader in that region.**

Flash only when you accept that change:

```sh
make flash
```

Open the ST-LINK serial port at **115200 baud, 8 data bits, no parity, and 1 stop bit**.
Send `STATUS` with a newline. Check that `outputs=0` and `applied=0` appear in the response.

The board needs no motor connection for the first serial test.

## Enable motor output later

Complete the electrical checks in the wiring guide first.
The following build permits motion after an ARM command:

```sh
make MOTOR_OUTPUT_ENABLED=1
```

The default duty limit is 25 percent. This limit does not limit motor current.
No command can enable the motor automatically after reset.

## Read next

- [Implementation plan](docs/implementation-plan.md)
- [Wiring and power checks](docs/wiring.md)
- [Tasks and resource ownership](docs/architecture.md)
- [Serial commands and faults](docs/protocol.md)
- [Test procedure and current results](docs/test-results.md)
- [Dependency notices](THIRD_PARTY_NOTICES.md)

The original sibling `MACH32/` repository remains separate and unchanged.
