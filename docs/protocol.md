# Serial commands and faults

## Connection

Use USART2 through the ST-LINK virtual COM port.
Set 115200 baud, 8 data bits, no parity, and 1 stop bit.
End each command with LF or CRLF.

Commands use uppercase ASCII.
Each input buffer holds at most 47 characters before its null terminator.
An overlong or damaged line cannot supply a valid command suffix.

## Commands

| Command | Effect |
|---|---|
| `STATUS` | Request a status record. Do not renew serial control. |
| `ARM SERIAL` | Enable serial mode with zero demand. |
| `ARM POT` | Enable forward control from the potentiometer. |
| `SET n` | Set signed demand from -100 to 100 in serial mode. |
| `KEEPALIVE` | Renew the timeout in active serial mode. |
| `STOP` | Disable the output and discard queued commands. |

ARM is valid only when the controller does not run.
After reset, the controller starts in DISABLED.
After a recoverable fault, another ARM command clears the fault.
A persistent fault condition can immediately produce another fault.

`SET n` controls duty demand, not measured speed.
A positive value selects forward. A negative value selects reverse.
POT mode only selects forward motion.

The board scales demand by `MOTOR_MAX_DUTY_PERCENT`.
At the default limit of 25, `SET 100` requests 25 percent duty.
At the same limit, `SET 20` requests 5 percent duty.
Integer rounding can produce zero duty for very small demands.

## Timeout

Serial control expires 500 ms after the last accepted ARM, SET, or KEEPALIVE command.
The timestamp comes from UART reception, not delayed queue processing.
Send KEEPALIVE at intervals shorter than 500 ms, such as 100 ms.

STATUS and invalid commands do not extend the timeout.
A late SET or KEEPALIVE cannot recover an expired session.
POT mode does not need a host heartbeat.

The control task checks expiration at its next activation.
The 520 ms disable target remains subject to hardware verification.

## Responses

- `QUEUED`: The command entered the control queue. This does not confirm a state change.
- `ERROR BUSY`: The controller rejected queue insertion or an emergency remains pending.
- `ERROR COMMAND`: The command format is invalid.
- `OK STOP`: The serial task disabled the output and requested STOP processing.
- `STATUS ...`: The current state and diagnostic fields.

Use STATUS to confirm the result of a queued command.
The `rejected` counter includes valid commands that the state machine refused.
The `invalid` counter includes invalid command lines.

The `outputs` field identifies the build configuration.
If `outputs=0`, no command can enable physical PWM.
The `output` field shows logical demand after state checks.
The `applied` field shows the signed duty command that the board layer applied.
It is not an independent electrical measurement.

If the transmit buffer fills, the driver drops a complete response record.
The `tx_drops` counter records those drops.
Response congestion cannot block motor control.

## Faults

| Fault | Cause | Recovery |
|---|---|---|
| `TIMEOUT` | Serial control expired | Explicit ARM |
| `ADC` | Invalid or stale input in POT mode | Restore input, then ARM |
| `RX_OVERFLOW` | UART receive error or full receive queue | Restore communication, then ARM |
| `COMMAND_QUEUE` | Full control queue | Reduce command load, then ARM |
| `TASK_STALL` | Required task stopped making progress | Watchdog reset |

STOP does not clear a fault.
STOP also invalidates normal commands already in the receive buffer.
Wait for the STOP result before you send ARM again.
Do not use this UART command as a certified emergency stop.

## Test hooks

The default build excludes these commands.
To compile them, use `ENABLE_TEST_HOOKS=1`.

```text
STALL CONTROL
STALL INPUT
STALL SERIAL
STALL DIAGNOSTICS
```

Each command suspends the selected task until reset.
Use the output-disabled build for these tests.
Do not enable test hooks in a normal motion build.
