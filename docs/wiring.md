# Wiring and power checks

## Do not connect motor power yet

The kit contains an L293D and a small DC motor.
The kit name does not establish the motor voltage, stall current, or battery capability.

Check these items before motion:

- Motor rated voltage and stall current.
- L293D current, voltage, and thermal limits.
- Battery voltage and available current.
- Breadboard regulator limits, if you use that module.
- Driver voltage loss at the expected current.

Do not connect a 9 V battery directly to an unidentified motor.
Do not use a duty limit as current protection.
Do not stall the motor to measure current without suitable equipment and current limiting.

## Board signals

These names identify MCU signals, not physical connector positions.
Check the NUCLEO-F446RE pin labels before connection.
Do not attach the Raspberry Pi ribbon cable to the STM32 board.

| STM32 signal | Function | Connection |
|---|---|---|
| PA6 / TIM3_CH1 | PWM enable | L293D pin 1 |
| PB0 | Direction input 1 | L293D pin 2 |
| PB1 | Direction input 2 | L293D pin 7 |
| PA0 / ADC1_IN0 | Potentiometer input | Potentiometer center terminal |
| PA2 / USART2_TX | Serial transmit | Existing ST-LINK virtual COM connection |
| PA3 / USART2_RX | Serial receive | Existing ST-LINK virtual COM connection |
| PC7 | Optional timing probe | Logic analyzer input only |
| GND | Signal reference | Driver and motor-supply ground |

The usual board routing connects USART2 to ST-LINK.
Check the solder bridges if you changed that routing.
The clock configuration expects the ST-LINK 8 MHz clock on the HSE input.

## L293D connections

Check the chip notch and pin numbers before insertion.
Disconnect all power before wiring.

| L293D pin | Connection |
|---|---|
| 1, EN1 | PA6 and a **10 kOhm resistor to GND** |
| 2, IN1 | PB0 |
| 3, OUT1 | Motor terminal 1 |
| 4, 5, 12, 13 | Common GND |
| 6, OUT2 | Motor terminal 2 |
| 7, IN2 | PB1 |
| 8, motor supply | Compatible motor supply after the electrical review |
| 16, logic supply | Regulated 5 V |
| 9, unused EN2 | GND |
| 10 and 15, unused inputs | GND |
| 11 and 14, unused outputs | No connection |

The L293D uses a 5 V logic supply. Its TTL-compatible inputs accept STM32 3.3 V control signals.
Check the actual chip markings and datasheet. A substitute driver can have different limits.

Place local supply decoupling near the driver, as specified by its datasheet.
The motor can produce electrical noise and supply drops.
Check the kit for suitable capacitors before motion tests.

The external resistor on EN1 keeps the driver disabled while the STM32 pins have no output configuration.
Firmware alone cannot control a pin during reset or loss of MCU power.

## Potentiometer

- Connect one outside terminal to STM32 3.3 V.
- Connect the other outside terminal to GND.
- Connect the center terminal to PA0.

Do not apply 5 V to PA0 in analog mode.
The STM32 ADC replaces the kit's external ADC for this project.

## Power arrangement

Power the STM32 board through its ST-LINK USB connector during initial tests.
Use a suitable motor supply after its rating review.
Connect the grounds together.

Do not power the motor from a GPIO pin or the STM32 3.3 V rail.
Do not connect two different supplies to the same breadboard power rail.
Check whether the breadboard has split power rails.
Do not connect an external UART device to ST-LINK TX without checking signal ownership.

Use a physical disconnect in the motor supply path.
Keep the shaft clear. Remove an exposed fan blade before tests.
Support the motor so that it cannot move across the bench.

## Motion-disabled test

The default firmware keeps PA6 low, even if the motor state reports RUNNING.
Test serial commands without connecting the motor or its supply.

## Motion-enabled test

After the circuit review, build with:

```sh
make MOTOR_OUTPUT_ENABLED=1
```

That command only builds the image. It does not flash the board.
Use the same build variables when you later run `make flash`.

The firmware generates PWM at 2 kHz.
The default maximum duty is 25 percent.
A `SET 20` command requests 20 percent of that limit, or 5 percent electrical duty.
The motor can fail to start at low duty. Do not leave a stalled motor powered.

The nominal 100 ms reversal delay gives an electrical off interval.
It does not prove that the shaft stopped before the direction changes.

## References

- [Freenove motor tutorial](https://docs.freenove.com/projects/fnk0020/en/latest/fnk0020/codes/c-lang/Motor%20%26%20Driver.html)
- [TI L293D datasheet](https://www.ti.com/lit/ds/symlink/l293.pdf)
- [ST NUCLEO-F446RE page](https://www.st.com/en/evaluation-tools/nucleo-f446re.html)
