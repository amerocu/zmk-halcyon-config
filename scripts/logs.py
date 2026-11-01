#!/usr/bin/env python3
"""Stream logs from all USB CDC-ACM logging devices simultaneously.

Devices are discovered continuously. Each opened connection gets a sequential
ID, including reconnects, so reused tty names cannot mix up device sessions.
"""

import glob
import os
import select
import sys
import termios
import time
from dataclasses import dataclass

SCAN_INTERVAL = 1.0

LABELS = {
    # Map a tty to a friendly name; unknown devices fall back to their basename.
}


def open_device(path):
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        attr = termios.tcgetattr(fd)
        # raw mode, 8N1, 115200 baud
        attr[0] = 0  # iflag
        attr[1] = 0  # oflag
        attr[2] = termios.CS8 | termios.CLOCAL | termios.CREAD  # cflag
        attr[3] = 0  # lflag
        attr[4] = termios.B115200  # ispeed
        attr[5] = termios.B115200  # ospeed
        attr[6][termios.VMIN] = 1
        attr[6][termios.VTIME] = 0
        termios.tcsetattr(fd, termios.TCSANOW, attr)
        # Preserve buffered boot messages when attaching to a new device.
    except BaseException:
        os.close(fd)
        raise
    return fd


def label_for(path):
    base = os.path.basename(path)
    return LABELS.get(base, base)


@dataclass
class Device:
    path: str
    id: int
    buffer: bytes = b""

    @property
    def label(self):
        return f"device-{self.id} {label_for(self.path)}"


class LogStreamer:
    def __init__(self):
        self.devices = {}
        self.failed_paths = set()
        self.next_id = 1

    def discover(self):
        paths = set(glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*"))
        self.failed_paths.intersection_update(paths)
        for fd, device in list(self.devices.items()):
            if device.path not in paths:
                self.disconnect(fd)

        active_paths = {device.path for device in self.devices.values()}
        for path in sorted(paths - active_paths):
            try:
                fd = open_device(path)
            except (OSError, termios.error) as exc:
                # Retry on each scan, but report a persistent failure only once.
                if path not in self.failed_paths:
                    if isinstance(exc, PermissionError):
                        print(f"Permission denied opening {path}. Try: sudo just logs",
                              file=sys.stderr, flush=True)
                    else:
                        print(f"Failed to open {path}: {exc}", file=sys.stderr, flush=True)
                self.failed_paths.add(path)
                continue
            self.failed_paths.discard(path)
            device = Device(path, self.next_id)
            self.next_id += 1
            self.devices[fd] = device
            print(f"[{device.label}] Connected ({path})", flush=True)

    def disconnect(self, fd, reason="device removed"):
        device = self.devices.pop(fd)
        if device.buffer:
            text = device.buffer.decode("utf-8", errors="replace").rstrip("\r")
            print(f"[{device.label}] {text} [partial line]", flush=True)
        self.close_fd(fd)
        print(f"[{device.label}] Disconnected ({reason})", flush=True)

    def read(self, fd):
        device = self.devices[fd]
        try:
            data = os.read(fd, 4096)
        except BlockingIOError:
            return
        except OSError as exc:
            self.disconnect(fd, str(exc))
            return
        if not data:
            self.disconnect(fd, "end of stream")
            return
        device.buffer += data
        while b"\n" in device.buffer:
            line, device.buffer = device.buffer.split(b"\n", 1)
            text = line.decode("utf-8", errors="replace").rstrip("\r")
            print(f"[{device.label}] {text}", flush=True)

    @staticmethod
    def close_fd(fd):
        try:
            os.close(fd)
        except OSError:
            pass

    def close(self):
        for fd in self.devices:
            self.close_fd(fd)
        self.devices.clear()


def main():
    print("Watching /dev/ttyACM* and /dev/ttyUSB* for log devices. "
          "Press Ctrl+C to stop.", flush=True)
    streamer = LogStreamer()
    next_scan = 0.0

    try:
        while True:
            now = time.monotonic()
            if now >= next_scan:
                streamer.discover()
                next_scan = now + SCAN_INTERVAL
            timeout = max(0.0, next_scan - time.monotonic())
            if not streamer.devices:
                time.sleep(timeout)
                continue
            readable, _, _ = select.select(list(streamer.devices), [], [], timeout)
            for fd in readable:
                streamer.read(fd)
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        streamer.close()


if __name__ == "__main__":
    main()
