#!/usr/bin/env python3
"""Check vector ownership and the absence of heap functions in a linked image."""

import argparse
from pathlib import Path
import struct
import subprocess


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--nm", default="arm-none-eabi-nm")
    args = parser.parse_args()
    output = subprocess.check_output(
        [args.nm, "--defined-only", "--format=posix", str(args.elf)], text=True
    )
    symbols = {}
    for line in output.splitlines():
        fields = line.split()
        if len(fields) >= 3:
            symbols[fields[0]] = (fields[1], int(fields[2], 16))
    image = args.binary.read_bytes()
    handlers = {
        "Reset_Handler": 1,
        "SVC_Handler": 11,
        "PendSV_Handler": 14,
        "SysTick_Handler": 15,
        "USART2_IRQHandler": 16 + 38,
    }
    for name, index in handlers.items():
        kind, address = symbols[name]
        vector = struct.unpack_from("<I", image, index * 4)[0]
        if vector != (address | 1):
            raise SystemExit(f"FAIL: {name} does not own its vector")
        if name != "Reset_Handler" and kind.upper() != "T":
            raise SystemExit(f"FAIL: {name} is not a strong code symbol")
    for name in ("malloc", "_malloc_r", "_sbrk", "pvPortMalloc", "mach32_systick_init"):
        if name in symbols:
            raise SystemExit(f"FAIL: unexpected linked function: {name}")
    stack = struct.unpack_from("<I", image)[0]
    if stack != 0x20020000:
        raise SystemExit("FAIL: unexpected initial stack address")
    print(f"PASS: exception vectors, UART vector, and no heap functions: {args.elf}")


if __name__ == "__main__":
    main()
