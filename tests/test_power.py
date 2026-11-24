"""Exercise production shutdown routing, display/pin ordering, and pixel buffers."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


@unittest.skipUnless(shutil.which("cc"), "requires a host C compiler")
class PowerTests(unittest.TestCase):
    def test_shutdown_lifecycle(self):
        root = Path(__file__).resolve().parents[1]
        fixtures = root / "tests/fixtures/power"
        sync = root / "boards/shields/mod_split_state_sync"
        headers = (
            "zephyr/device.h", "zephyr/kernel.h", "zephyr/init.h",
            "zephyr/logging/log.h", "zephyr/sys/atomic.h", "zephyr/drivers/gpio.h",
            "drivers/behavior.h", "zmk/behavior.h", "zmk/pm.h",
            "zmk/event_manager.h", "zmk/events/activity_state_changed.h",
        )
        with tempfile.TemporaryDirectory(prefix="halcyon-power-") as directory:
            temp = Path(directory)
            for header in headers:
                path = temp / header
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('#include "mock.h"\n')
            for central, display in ((1, 0), (0, 0), (0, 1)):
                with self.subTest(central=central, display=display):
                    executable = temp / f"check-{central}-{display}"
                    compiled = subprocess.run([
                        "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-Wno-unused-parameter", f"-DCONFIG_ZMK_SPLIT_ROLE_CENTRAL={central}",
                        f"-DTEST_DISPLAY={display}",
                        "-I", str(temp), "-I", str(fixtures), "-I", str(sync),
                        str(fixtures / "check.c"), "-o", str(executable),
                    ], capture_output=True, text=True, timeout=30)
                    self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
                    checked = subprocess.run([str(executable)], capture_output=True,
                                             text=True, timeout=10)
                    self.assertEqual(checked.returncode, 0, checked.stdout + checked.stderr)


if __name__ == "__main__":
    unittest.main()
