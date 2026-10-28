#!/usr/bin/env python3
"""Stream logs from all USB CDC-ACM logging devices simultaneously.

Each device's output is prefixed with its tty name so you can tell the dongle,
left, and right halves apart.
"""

import glob
import os
import select
import sys
import termios

# Linux termios baud constant for 115200 (not exported by Python's termios).
B115200 = 4098

LABELS = {
    # Map a tty to a friendly name; unknown devices fall back to their basename.
}


def open_device(path):
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY)
    attr = termios.tcgetattr(fd)
    # raw mode, 8N1, 115200 baud
    attr[0] = 0  # iflag
    attr[1] = 0  # oflag
    attr[2] = termios.CS8 | termios.CLOCAL | termios.CREAD  # cflag
    attr[3] = 0  # lflag
    attr[4] = B115200  # ispeed
    attr[5] = B115200  # ospeed
    attr[6][termios.VMIN] = 1
    attr[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attr)
    termios.tcflush(fd, termios.TCIOFLUSH)
    return fd


def label_for(path):
    base = os.path.basename(path)
    return LABELS.get(base, base)


def main():
    paths = sorted(glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*"))
    if not paths:
        print("No /dev/ttyACM* or /dev/ttyUSB* devices found.", file=sys.stderr)
        sys.exit(1)

    fds = {}
    for path in paths:
        try:
            fds[open_device(path)] = path
        except PermissionError:
            print(f"Permission denied opening {path}. Try: sudo just logs",
                  file=sys.stderr)
        except OSError as exc:
            print(f"Failed to open {path}: {exc}", file=sys.stderr)

    if not fds:
        sys.exit(1)

    print(f"Streaming logs from: {', '.join(label_for(p) for p in fds.values())}")
    print("Press Ctrl+C to stop.", flush=True)

    bufs = {fd: b"" for fd in fds}
    try:
        while True:
            readable, _, _ = select.select(list(fds.keys()), [], [], 1.0)
            for fd in readable:
                try:
                    data = os.read(fd, 4096)
                except OSError:
                    continue
                if not data:
                    continue
                bufs[fd] += data
                while b"\n" in bufs[fd]:
                    line, bufs[fd] = bufs[fd].split(b"\n", 1)
                    text = line.decode("utf-8", errors="replace").rstrip("\r")
                    sys.stdout.write(f"[{label_for(fds[fd])}] {text}\n")
                sys.stdout.flush()
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        for fd in fds:
            try:
                os.close(fd)
            except OSError:
                pass


if __name__ == "__main__":
    main()
