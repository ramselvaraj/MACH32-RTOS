# Implementation plan

## Goal

Build a small motor controller with FreeRTOS and MACH32.
Prove the behavior through tests and measurements.

The existing kit supplies the motor, L293D, potentiometer, breadboard, and wires.
The STM32 board runs the firmware.
Electrical compatibility still needs confirmation.

## Stages

| Stage | Deliverable | Current status |
|---|---|---|
| 1. Repository | Independent Git repository and fixed dependency revisions | Complete. Dependencies use fixed revisions. |
| 2. Build | Firmware images for the STM32F446RE | All four output/test-hook configurations build. |
| 3. Kernel | Static tasks, queues, interrupt priorities, and exception handlers | Implemented. Board execution remains unverified. |
| 4. Control | Motor states, two input modes, STOP, timeout, and reversal delay | Implemented. Host tests pass. |
| 5. Supervision | Task progress checks and independent watchdog | Implemented. Reset behavior needs a board test. |
| 6. Verification | Host tests, hardware test script, and CI configuration | Host tests pass. Hardware tests and remote CI remain pending. |
| 7. Wiring | Checked power source and motor circuit | Guide provided. Electrical review remains pending. |
| 8. Measurements | Timing captures, stack margins, and load-test results | Pending hardware access. |

## First physical test

1. Confirm the motor voltage and current ratings.
2. Confirm the power source and driver limits.
3. Fit the external enable pull-down resistor.
4. Keep motor power disconnected.
5. Test the output-disabled firmware through ST-LINK serial.
6. Measure the enable signal during power application and reset.
7. Enable motion only after the circuit passes these checks.

## Completion criteria

- A fresh checkout builds with fixed dependency revisions.
- Reset does not enable the motor.
- STOP disables the output during serial traffic.
- Serial control expires after 500 ms without a valid control command.
- The measured timeout-to-disable response meets the 520 ms initial target.
- Stalled tasks cause output disable and watchdog recovery.
- No fault recovery starts motion without another ARM command.
- The test report identifies the firmware revision, circuit, workload, and instruments.

The 520 ms value is a proposed acceptance target. It is not a measured result.

## Later work

Add an encoder for measured speed control.
Add CAN after the first version passes its tests.
Keep bootloader work separate from this version.

## Resume statement

Use the following statement only after the board tests pass:

> Developed an STM32 motor controller with FreeRTOS and custom MACH32 drivers for PWM, ADC, UART, and watchdog control.

> Automated hardware tests for command timeouts, task stalls, and reset recovery. Measured task timing under serial traffic.

Add measured values only after the test report contains supporting evidence.
