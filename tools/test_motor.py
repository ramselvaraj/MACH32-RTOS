#!/usr/bin/env python3
"""Run serial bench checks. This is not an electrical timing measurement."""

from __future__ import annotations

import argparse
from collections.abc import Callable
from datetime import datetime, timezone
import json
from pathlib import Path
import time
from typing import Any


class BenchError(RuntimeError):
    pass


def parse_status(line: str) -> dict[str, str] | None:
    if not line.startswith("STATUS "):
        return None
    result: dict[str, str] = {}
    for field in line.split()[1:]:
        key, separator, value = field.partition("=")
        if not separator or not key or not value or key in result:
            raise BenchError(f"Invalid status field: {field!r}")
        result[key] = value
    required = {
        "state", "mode", "fault", "target", "output", "applied", "outputs",
        "hooks", "uptime_ms", "reset", "invalid", "rejected",
    }
    if not required <= result.keys():
        raise BenchError("Incomplete status record")
    if result["state"] not in {"DISABLED", "RUNNING", "FAULT"}:
        raise BenchError("Invalid motor state")
    if result["mode"] not in {"SERIAL", "POT"}:
        raise BenchError("Invalid input mode")
    if result["outputs"] not in {"0", "1"} or result["hooks"] not in {"0", "1"}:
        raise BenchError("Invalid firmware configuration")
    for key in ("target", "output", "applied", "uptime_ms", "invalid", "rejected"):
        try:
            int(result[key])
        except ValueError as error:
            raise BenchError(f"Invalid integer in {key}") from error
    try:
        int(result["reset"], 0)
    except ValueError as error:
        raise BenchError("Invalid reset flags") from error
    return result


def check_motion_permission(status: dict[str, str], allow_motion: bool) -> None:
    if status["outputs"] != "0" and not allow_motion:
        raise BenchError(
            "This firmware can drive the motor. Disconnect motor power or use the "
            "default output-disabled firmware. Use --allow-motion only after a wiring check."
        )


class Bench:
    def __init__(self, port: Any) -> None:
        self.port = port
        self.pending = bytearray()
        self.records: list[dict[str, Any]] = []

    def send(self, command: str) -> None:
        self.port.write((command + "\n").encode("ascii"))

    def line(self, deadline: float) -> str | None:
        while time.monotonic() < deadline:
            if b"\n" in self.pending:
                raw, _, rest = self.pending.partition(b"\n")
                self.pending = bytearray(rest)
                return raw.decode("ascii", errors="replace").rstrip("\r")
            self.pending.extend(self.port.readline())
            if len(self.pending) > 8192:
                raise BenchError("The serial record exceeds the size limit")
        return None

    def wait_status(
        self, predicate: Callable[[dict[str, str]], bool] = lambda _: True,
        timeout: float = 2.0,
    ) -> dict[str, str]:
        deadline = time.monotonic() + timeout
        last_request = 0.0
        while time.monotonic() < deadline:
            if time.monotonic() - last_request >= 0.12:
                self.send("STATUS")
                last_request = time.monotonic()
            line = self.line(min(deadline, time.monotonic() + 0.15))
            if line is None:
                continue
            status = parse_status(line)
            if status is not None:
                self.records.append({"host_monotonic_s": time.monotonic(), **status})
                if predicate(status):
                    return status
        raise BenchError("Timed out while waiting for the expected status")


def run_suite(bench: Bench, allow_motion: bool, stall_test: bool) -> list[str]:
    passed: list[str] = []
    initial = bench.wait_status()
    check_motion_permission(initial, allow_motion)
    if stall_test and initial["hooks"] != "1":
        raise BenchError("The stall test needs ENABLE_TEST_HOOKS=1")

    bench.send("STOP")
    bench.wait_status(lambda s: s["state"] != "RUNNING" and s["applied"] == "0")
    passed.append("STOP disables the reported output")

    bench.send("ARM SERIAL")
    armed = bench.wait_status(lambda s: s["state"] == "RUNNING" and s["target"] == "0")
    passed.append("ARM SERIAL starts with zero demand")

    bench.send("SET 101")
    invalid_before = int(armed["invalid"])
    bench.wait_status(lambda s: int(s["invalid"]) > invalid_before and s["target"] == "0")
    passed.append("Out-of-range commands do not change the demand")

    bench.send("SET 20")
    bench.wait_status(lambda s: s["state"] == "RUNNING" and s["output"] == "20")
    end = time.monotonic() + 0.8
    while time.monotonic() < end:
        bench.send("KEEPALIVE")
        time.sleep(0.08)
    bench.wait_status(lambda s: s["state"] == "RUNNING")
    passed.append("KEEPALIVE maintains serial control")

    # STATUS does not renew the command timeout. Poll until the board reports it.
    bench.wait_status(lambda s: s["state"] == "FAULT" and s["fault"] == "TIMEOUT")
    bench.send("SET 20")
    bench.wait_status(lambda s: s["state"] == "FAULT" and s["output"] == "0")
    passed.append("Expired control requires an explicit ARM command")

    bench.send("ARM SERIAL")
    bench.send("SET 20")
    bench.wait_status(lambda s: s["state"] == "RUNNING" and s["output"] == "20")
    bench.send("STOP")
    bench.wait_status(lambda s: s["state"] == "DISABLED" and s["applied"] == "0")
    passed.append("STOP works during serial control")

    if stall_test:
        # Use a zero demand. A stalled task must never resume without reset.
        before = bench.wait_status()
        bench.send("STALL CONTROL")
        bench.wait_status(
            lambda s: s["state"] == "DISABLED"
            and int(s["uptime_ms"]) < int(before["uptime_ms"])
            and (int(s["reset"], 0) & (1 << 29)) != 0,
            timeout=5.0,
        )
        passed.append("A control-task stall causes an independent-watchdog reset")
    return passed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="For example /dev/ttyACM0 or COM5")
    parser.add_argument("--allow-motion", action="store_true")
    parser.add_argument("--stall-test", action="store_true", help="Intentionally reset the board")
    parser.add_argument("--output", type=Path, help="Path for the JSON test record")
    args = parser.parse_args()
    try:
        import serial
    except ImportError:
        parser.error("Install the tool dependency: python -m pip install -r tools/requirements.txt")

    timestamp = datetime.now(timezone.utc)
    output = args.output or Path("test-results") / f"bench-{timestamp:%Y%m%dT%H%M%SZ}.json"
    record: dict[str, Any] = {
        "utc": timestamp.isoformat(), "port": args.port,
        "allow_motion": args.allow_motion, "stall_test": args.stall_test,
        "measurement_scope": "Serial-reported state. No independent electrical timing measurement.",
        "passed": [], "success": False,
    }
    bench: Bench | None = None
    try:
        with serial.Serial(args.port, 115200, timeout=0.05, write_timeout=1.0) as port:
            bench = Bench(port)
            try:
                record["passed"] = run_suite(bench, args.allow_motion, args.stall_test)
                record["success"] = True
            finally:
                # End the session with a STOP even if an assertion fails.
                bench.send("STOP")
    except (BenchError, OSError, serial.SerialException) as error:
        record["success"] = False
        record["error"] = str(error)
    finally:
        record["status_records"] = bench.records if bench is not None else []
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    print(f"{'PASS' if record['success'] else 'FAIL'}: {output}")
    if "error" in record:
        print(record["error"])
    return 0 if record["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
