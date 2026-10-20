# Test procedure and current results

## Current status

Date: 2026-10-20.

The local host tests pass. All four firmware configurations compile and link.
No board was flashed during implementation.
No physical motor, reset, timing, or electrical test has passed yet.
The CI workflow exists but has not run on GitHub.

## Local checks

```sh
make test
make test-sanitize
make check-python
make -j4 verify
make -j4 verify ENABLE_TEST_HOOKS=1
make -j4 verify MOTOR_OUTPUT_ENABLED=1
make -j4 verify MOTOR_OUTPUT_ENABLED=1 ENABLE_TEST_HOOKS=1
```

| Check | Result |
|---|---|
| C tests without test hooks | 12 groups pass |
| C tests with test hooks | 12 groups pass |
| AddressSanitizer and UndefinedBehaviorSanitizer | 12 groups pass |
| Python tests for the bench tool | 10 tests pass |
| Python syntax checks | Pass |
| Default firmware build | Pass |
| Three additional build configurations | Pass |
| Linked exception vectors, UART vector, and heap-symbol checks | Pass in all four configurations |

Local tools: Arm GCC 16.2.0, host GCC 16.2.1, and Python 3.14.7.

The default image reports 15,800 bytes of text, 8 bytes of initialized data, and 16,992 bytes of BSS.
BSS includes the static task stacks and buffers.
These figures do not include the interrupt stack's runtime use or establish stack margin.

The C tests cover state transitions, timeout boundaries, command age, direction changes, time wrap, and supervision.
They also cover invalid commands, line recovery, bounded formatting, and 100,000 pseudo-random input bytes.
These are host tests. They do not execute the Cortex-M4F scheduler or peripheral registers.

## Automated board tests

Keep motor power disconnected for the first test.
Use the default output-disabled firmware.

Create an isolated Python environment:

```sh
python3 -m venv .venv
```

Install the serial dependency:

```sh
.venv/bin/python -m pip install -r tools/requirements.txt
```

Run the bench checks:

```sh
.venv/bin/python tools/test_motor.py /dev/ttyACM0
```

Use the correct port name for your computer.
On Windows, use `.venv\Scripts\python.exe` and a port such as `COM5`.
Close other programs that use the same serial port.

The script checks STOP, zero-demand arming, invalid commands, KEEPALIVE, timeout, and explicit rearming.
It saves a JSON record under `test-results/`.
It refuses motion-enabled firmware unless you supply `--allow-motion`.
That option permits motor commands. Use it only after the electrical review.

For the task-stall test, build and flash output-disabled firmware with test hooks.
Then run:

```sh
.venv/bin/python tools/test_motor.py /dev/ttyACM0 --stall-test
```

This test deliberately resets the board through the independent watchdog.
It checks the reset flag and the new uptime.
The script checks serial-reported state. It does not prove physical output levels or electrical response time.

## Physical acceptance tests

| Test | Initial target | Result |
|---|---|---|
| Power application and reset | EN1 remains low until explicit ARM | Pending |
| STOP under serial traffic | EN1 goes low. Record the actual latency. | Pending |
| Serial command expiry | EN1 goes low within 520 ms of the last valid control command | Pending |
| Direction reversal | Nominal 100 ms commanded off interval. Record the electrical interval. | Pending |
| Task stall | Software disables output after the progress deadline, then watchdog resets | Pending |
| Scheduler stall | Watchdog resets with output disabled after reset | Pending |
| Load test | No missed scheduled periods during a defined 30-minute workload | Pending |
| Memory margin | Record minimum unused words for each task | Pending |
| Receive overload | Record overflow handling and parser recovery | Pending |

Use a logic analyzer or oscilloscope for electrical timing tests.
Measure UART input and PA6 together for timeout and STOP tests.
Use PC7 to observe the control work region.
Disconnect motor power during fault tests when possible.

For reset tests, record PA6 before, during, and after reset.
Check the external enable pull-down resistor independently of firmware.
Do not infer a physical stop from an enable-pin measurement.

## Evidence to record

- Main repository revision and dependency revisions.
- Compiler version and all build variables.
- Board revision, wiring, motor rating, and power source.
- Test duration and serial workload.
- Instrument model, sample rate, and timestamp resolution.
- Raw captures, JSON records, faults, and observed maxima.

Do not report observed maxima as proven worst-case limits.
Do not place unmeasured performance values on a resume.
