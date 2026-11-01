"""Run the production battery relay/receiver on the host with AddressSanitizer."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


@unittest.skipUnless(shutil.which("cc"), "requires a host C compiler with AddressSanitizer")
class SplitBatteryTests(unittest.TestCase):
    def test_uuid_lifetime_and_connection_transitions(self):
        root = Path(__file__).resolve().parents[1]
        fixtures = root / "tests/fixtures/split_battery"
        module = root / "boards/shields/mod_split_state_sync"
        headers = (
            "zephyr/kernel.h", "zephyr/init.h", "zephyr/device.h", "zephyr/sys/util.h",
            "zephyr/bluetooth/conn.h", "zephyr/bluetooth/gatt.h", "zephyr/bluetooth/uuid.h",
            "zephyr/logging/log.h", "zmk/ble.h", "zmk/event_manager.h", "zmk/behavior.h",
            "zmk/split/central.h", "zmk/split/bluetooth/peripheral.h",
            "zmk/events/battery_state_changed.h", "zmk/events/split_peripheral_status_changed.h",
            "drivers/behavior.h",
            "zmk/usb.h",
        )
        with tempfile.TemporaryDirectory(prefix="split-battery-") as directory:
            temp = Path(directory)
            for header in headers:
                path = temp / header
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('#include "mock.h"\n')
            executable = temp / "check"
            compiled = subprocess.run([
                "cc", "-std=c11", "-g", "-O1", "-fsanitize=address",
                "-fsanitize-address-use-after-scope", "-fno-omit-frame-pointer",
                "-I", str(temp), "-I", str(fixtures), "-I", str(module),
                str(fixtures / "check.c"), "-o", str(executable),
            ], capture_output=True, text=True, timeout=30)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            checked = subprocess.run([str(executable)], capture_output=True, text=True, timeout=10)
            self.assertEqual(checked.returncode, 0, checked.stdout + checked.stderr)
            self.assertIn("PASS: UUID lifetime", checked.stdout)


if __name__ == "__main__":
    unittest.main()
