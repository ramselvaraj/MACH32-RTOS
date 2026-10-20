import importlib.util
from pathlib import Path
import time
import unittest


SPEC = importlib.util.spec_from_file_location(
    "bench_tool", Path(__file__).resolve().parents[1] / "tools" / "test_motor.py"
)
bench_tool = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(bench_tool)

STATUS = (
    "STATUS state=DISABLED mode=SERIAL fault=NONE target=0 output=0 applied=0 "
    "outputs=0 hooks=0 uptime_ms=100 reset=0x00000000 invalid=0 rejected=0"
)


class FakePort:
    def __init__(self, chunks):
        self.chunks = list(chunks)
        self.writes = []

    def readline(self):
        return self.chunks.pop(0) if self.chunks else b""

    def write(self, data):
        self.writes.append(data)


class BenchToolTests(unittest.TestCase):
    def test_parse_status(self):
        result = bench_tool.parse_status(STATUS)
        self.assertEqual(result["state"], "DISABLED")
        self.assertEqual(result["outputs"], "0")

    def test_ignore_non_status(self):
        self.assertIsNone(bench_tool.parse_status("QUEUED"))

    def test_reject_missing_fields(self):
        with self.assertRaises(bench_tool.BenchError):
            bench_tool.parse_status("STATUS state=DISABLED")

    def test_reject_duplicate_fields(self):
        with self.assertRaises(bench_tool.BenchError):
            bench_tool.parse_status(STATUS + " outputs=0")

    def test_reject_invalid_numbers(self):
        with self.assertRaises(bench_tool.BenchError):
            bench_tool.parse_status(STATUS.replace("uptime_ms=100", "uptime_ms=bad"))

    def test_refuse_motion_without_permission(self):
        status = bench_tool.parse_status(STATUS.replace("outputs=0", "outputs=1"))
        with self.assertRaises(bench_tool.BenchError):
            bench_tool.check_motion_permission(status, False)
        bench_tool.check_motion_permission(status, True)

    def test_output_disabled_needs_no_motion_permission(self):
        bench_tool.check_motion_permission(bench_tool.parse_status(STATUS), False)

    def test_partial_and_multiple_lines(self):
        port = FakePort([b"STA", b"TUS\r\nQUEUED\n"])
        bench = bench_tool.Bench(port)
        deadline = time.monotonic() + 1.0
        self.assertEqual(bench.line(deadline), "STATUS")
        self.assertEqual(bench.line(deadline), "QUEUED")

    def test_status_wait(self):
        port = FakePort([b"QUEUED\n", (STATUS + "\r\n").encode()])
        bench = bench_tool.Bench(port)
        result = bench.wait_status()
        self.assertEqual(result["state"], "DISABLED")
        self.assertIn(b"STATUS\n", port.writes)
        self.assertEqual(len(bench.records), 1)

    def test_timeout(self):
        with self.assertRaises(bench_tool.BenchError):
            bench_tool.Bench(FakePort([])).wait_status(timeout=0.001)


if __name__ == "__main__":
    unittest.main()
