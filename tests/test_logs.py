"""Hot-plug and stream handling checks; see tests/README.md for usage."""

import errno
import io
import unittest
from contextlib import redirect_stderr, redirect_stdout
from unittest.mock import patch

from scripts import logs


class LogStreamerTests(unittest.TestCase):
    def setUp(self):
        self.paths = []
        self.output = io.StringIO()
        self.errors = io.StringIO()
        self.enterContext(redirect_stdout(self.output))
        self.enterContext(redirect_stderr(self.errors))
        self.enterContext(patch.object(logs.glob, "glob", side_effect=self.glob_paths))
        self.open_device = self.enterContext(patch.object(logs, "open_device"))
        self.close_fd = self.enterContext(patch.object(logs.os, "close"))
        self.read_fd = self.enterContext(patch.object(logs.os, "read"))
        self.streamer = logs.LogStreamer()
        self.addCleanup(self.streamer.close)

    def glob_paths(self, pattern):
        return [path for path in self.paths if path.startswith(pattern[:-1])]

    def test_late_arrival_new_device_and_reconnect_get_sequential_ids(self):
        self.streamer.discover()
        self.assertFalse(self.streamer.devices)
        self.open_device.side_effect = [10, 11, 12]
        self.paths = ["/dev/ttyACM0"]
        self.streamer.discover()
        self.paths.append("/dev/ttyUSB0")
        self.streamer.discover()
        self.streamer.discover()
        self.assertEqual(self.open_device.call_count, 2)
        self.paths.remove("/dev/ttyACM0")
        self.streamer.discover()
        self.close_fd.assert_called_once_with(10)
        self.paths.append("/dev/ttyACM0")
        self.streamer.discover()
        self.assertEqual(self.streamer.devices[12].id, 3)
        self.assertEqual(self.streamer.devices[11].id, 2)
        self.assertIn("[device-1 ttyACM0] Connected", self.output.getvalue())
        self.assertIn("[device-2 ttyUSB0] Connected", self.output.getvalue())
        self.assertIn("[device-1 ttyACM0] Disconnected", self.output.getvalue())
        self.assertIn("[device-3 ttyACM0] Connected", self.output.getvalue())

    def test_partial_lines_do_not_leak_into_a_reused_fd(self):
        self.paths = ["/dev/ttyACM0"]
        self.open_device.return_value = 10
        self.streamer.discover()
        self.read_fd.side_effect = [b"battery ", b"73%\r\nunfinished", b"", b"new session\n"]
        self.streamer.read(10)
        self.streamer.read(10)
        self.assertIn("[device-1 ttyACM0] battery 73%\n", self.output.getvalue())
        self.streamer.read(10)
        self.assertFalse(self.streamer.devices)
        self.assertIn("unfinished [partial line]", self.output.getvalue())
        self.streamer.discover()
        self.streamer.read(10)
        self.assertIn("[device-2 ttyACM0] new session\n", self.output.getvalue())
        self.assertNotIn("unfinishednew", self.output.getvalue())

    def test_hot_unplug_read_error_does_not_stop_other_devices(self):
        self.paths = ["/dev/ttyACM0", "/dev/ttyACM1"]
        self.open_device.side_effect = [10, 11]
        self.streamer.discover()
        self.read_fd.side_effect = [OSError(errno.EIO, "device removed"), b"still alive\n"]
        self.streamer.read(10)
        self.streamer.read(11)
        self.assertNotIn(10, self.streamer.devices)
        self.assertIn("[device-2 ttyACM1] still alive", self.output.getvalue())

    def test_permission_failure_retries_without_spam_or_consuming_ids(self):
        self.paths = ["/dev/ttyACM0"]
        self.open_device.side_effect = [PermissionError(), PermissionError(), 10]
        self.streamer.discover()
        self.streamer.discover()
        self.streamer.discover()
        self.assertEqual(self.errors.getvalue().count("Permission denied"), 1)
        self.assertEqual(self.streamer.devices[10].id, 1)

    def test_nonblocking_read_keeps_device_connected(self):
        self.paths = ["/dev/ttyACM0"]
        self.open_device.return_value = 10
        self.streamer.discover()
        self.read_fd.side_effect = BlockingIOError()
        self.streamer.read(10)
        self.assertIn(10, self.streamer.devices)
        self.close_fd.assert_not_called()

    def test_terminal_setup_failure_is_retried(self):
        self.paths = ["/dev/ttyACM0"]
        self.open_device.side_effect = [logs.termios.error("unplugged"), 10]
        self.streamer.discover()
        self.streamer.discover()
        self.assertIn("Failed to open", self.errors.getvalue())
        self.assertEqual(self.streamer.devices[10].id, 1)

    def test_main_discovers_arrivals_while_an_existing_device_is_logging(self):
        self.paths = ["/dev/ttyACM0"]
        self.open_device.side_effect = [10, 11]
        self.read_fd.return_value = b"existing device log\n"
        selected = []

        def select_ready(fds, *_args):
            selected.append(fds)
            if len(selected) == 1:
                self.paths.append("/dev/ttyACM1")
                return [10], [], []
            raise KeyboardInterrupt()

        with patch.object(logs.time, "monotonic", side_effect=[0.0, 0.0, 1.1, 1.1]), \
                patch.object(logs.select, "select", side_effect=select_ready):
            logs.main()
        self.assertEqual(selected, [[10], [10, 11]])
        self.assertEqual({call.args[0] for call in self.close_fd.call_args_list}, {10, 11})
        self.assertIn("[device-2 ttyACM1] Connected", self.output.getvalue())

    def test_main_waits_with_no_devices_and_closes_on_interrupt(self):
        with patch.object(logs.time, "sleep", side_effect=KeyboardInterrupt()):
            logs.main()
        self.assertIn("Watching", self.output.getvalue())
        self.assertIn("Stopped.", self.output.getvalue())

    def test_failed_terminal_configuration_closes_fd(self):
        # Exercise the real opener, separate from the mocked discovery opens.
        with patch.object(logs.os, "open", return_value=99), \
                patch.object(logs.termios, "tcgetattr", side_effect=OSError("unplugged")):
            with self.assertRaises(OSError):
                REAL_OPEN_DEVICE("/dev/ttyACM0")
        self.close_fd.assert_called_once_with(99)


REAL_OPEN_DEVICE = logs.open_device


if __name__ == "__main__":
    unittest.main()
