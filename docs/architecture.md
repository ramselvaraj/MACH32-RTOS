# Tasks and resource ownership

## Hardware and kernel

The target is the NUCLEO-F446RE with an STM32F446RE Cortex-M4F.
The build uses the FreeRTOS GCC ARM_CM4F port and hard-float compiler settings.

MACH32 configures the 180 MHz processor clock.
APB1 runs at 45 MHz. Its timer clock runs at 90 MHz.

| Resource | Owner | Configuration |
|---|---|---|
| SysTick | FreeRTOS | 1 kHz kernel tick |
| SVC and PendSV | FreeRTOS | Context switching |
| TIM2 | Board layer | Free-running 1 MHz counter |
| TIM3 channel 1 | Control task through board layer | 2 kHz PWM |
| ADC1 channel 0 | Input task | One bounded conversion every 10 ms |
| USART2 receive | UART interrupt and serial task | Static queue, 256 timestamped bytes |
| USART2 transmit | UART interrupt | Fixed ring buffer, 2047 usable bytes |
| IWDG | Supervisor | Nominal 1 s timeout |

The build does not link MACH32's SysTick implementation.
The build does not call its blocking UART read or write functions.
The ADC wait has a 200 microsecond timer limit. No DMA path forms part of version 1.

## Tasks

A larger FreeRTOS priority number means a higher task priority.

| Task | Priority | Stack, words | Activation |
|---|---:|---:|---|
| Supervisor | 5 | 256 | Every 20 ms |
| Control | 4 | 512 | Every 10 ms or on command notification |
| Serial | 3 | 512 | Bounded batch, then a 2 ms delay |
| Input | 2 | 256 | Every 10 ms |
| Diagnostics | 1 | 1024 | Every 100 ms |
| Kernel idle | 0 | 128 | When other tasks wait |

Each stack word occupies four bytes.
Diagnostics sends a periodic report every second. STATUS requests can trigger an earlier report.
The diagnostics task has no blocking UART writes.

The kernel allocates all tasks and queues from static storage.
The application does not use malloc, a FreeRTOS heap, or printf.
A bounded formatter creates status records without dynamic allocation.

## Commands and STOP

The UART interrupt queues each received byte with its kernel timestamp.
It also records an emergency generation number.
The serial task checks complete lines and parses exact commands.
Normal control commands enter a queue with eight entries.

Only the control task can enable the physical motor output.
STOP does not enter the normal command queue.
It immediately disables the enable pin after the serial task recognizes the complete command.
It also notifies the control task and invalidates queued commands.
Commands received before STOP or a receive fault cannot rearm the output afterward.
Wait for the STOP result before you send another ARM command.

The final output write and emergency checks share a short critical section.
That section prevents an interrupt or task from disabling the output just before a stale control write enables it again.

An explicit ARM command selects the input mode.
Serial control starts with zero demand.
Potentiometer control uses the current knob position and only drives forward.
Stop the motor before a mode change.

## Interrupt rules

The NVIC configuration uses all four priority bits for preemption.
USART2 uses priority 6.
The FreeRTOS syscall threshold uses priority 5.
SysTick and PendSV use priority 15.

For interrupts, a smaller number means greater urgency.
USART2 can call the FreeRTOS FromISR APIs because its urgency permits kernel calls.

UART overrun, framing errors, and queue overflow disable the output and request a fault.
The parser discards damaged input until a new newline arrives.
Normal malformed commands do not renew serial control.

## Watchdog supervision

Control, input, serial, and diagnostics tasks increment separate progress counters.
The supervisor checks those counters every 20 ms.
If a counter remains unchanged for 300 ms, the supervisor disables the output and stops watchdog refreshes.

The task-stall fault remains latched until reset.
An ARM command cannot bypass it.
The independent watchdog uses the internal LSI clock.
Its timeout has oscillator tolerance and needs a physical measurement.

If the scheduler or interrupts stop completely, the supervisor cannot run.
The independent watchdog then provides reset recovery, not a guaranteed 300 ms software response.

## Measurements

- `max_control_us`: Maximum observed duration of the instrumented control region.
- `max_late_ms`: Maximum scheduled-release lateness, with 1 ms tick resolution.
- `missed`: Number of complete 10 ms periods skipped by late scheduled releases.
- `stacks`: Minimum unused stack words for control, input, serial, diagnostics, and supervisor.
- `queue_peak`: Maximum observed number of queued control commands.

The control duration can include higher-priority preemption.
It excludes task-wait time and some final bookkeeping.
These values do not establish mathematical worst-case execution time.

PC7 remains high during the control work region.
Use an external logic analyzer to compare that signal with UART and PA6.
The TIM2 counter wraps after about 71 minutes. Unsigned duration differences handle that wrap.
The 32-bit kernel tick wraps after about 49 days.

## Limitations

Tasks share memory. FreeRTOS does not isolate these tasks from each other.
The project has no encoder, current sensor, electrical fault input, or communication authentication.
The motor driver and supply need independent electrical review.

The firmware cannot detect a disconnected potentiometer from its ADC value alone.
The firmware cannot confirm actual shaft speed or physical stopping time.
The software does not establish functional-safety compliance.
