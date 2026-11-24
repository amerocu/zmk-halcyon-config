# Host-side tests

This directory contains regression tests for the log streamer, split battery
sync, and power transitions. They run without a keyboard, flashing, or a Zephyr SDK.
The operational tools remain in `scripts/`.

## Requirements

- Python 3.11 or newer; the Python tests use only the standard library.
- For battery tests, a native host compiler available as `cc` with AddressSanitizer
  support (for example, GCC or Clang with its sanitizer runtime installed).
  Use a compiler that produces executables for your computer, not ARM firmware.
- Power tests also need `cc`, but do not require AddressSanitizer.

If `cc` is missing, the battery suite is reported as skipped. If it is present
but cannot build or run AddressSanitizer programs, the test fails with compiler
or runtime output so the missing dependency is visible.

## Running tests

Run these commands from the repository root.

All suites:

```sh
python3 -B -m unittest discover -s tests -p 'test_*.py'
```

Log streamer only:

```sh
python3 -B -m unittest discover -s tests -p 'test_logs.py'
```

Split battery sync only:

```sh
python3 -B -m unittest discover -s tests -p 'test_split_battery.py'
```

Add `-v` to a command for individual test names. `-B` avoids creating Python
bytecode caches in the repository.

## What the tests cover

### Log streamer — `test_logs.py`

Imports the real `scripts/logs.py` and mocks device discovery, serial I/O, and
time. Checks include:

- Devices arriving after startup or while another device is logging.
- Sequential IDs for new connections and reconnects, including reused tty names.
- Disconnects, read errors, and partial-line handling.
- Permission/setup failures, retries, and cleanup on interruption.

No real serial devices or sudo privileges are required.

### Split battery sync — `test_split_battery.py`

Compiles and runs the production split-sync C code on the host with
AddressSanitizer, using the adapters in `fixtures/split_battery/`:

- **`check.c`** includes the production identity, relay, receiver, and USB-power
  service implementations and drives connection/state transitions through them.
- **`mock.h`** supplies lightweight Zephyr/ZMK interfaces for BLE connections,
  GATT reads, work scheduling, USB power, and event delivery. Its UUID definitions
  retain Zephyr's compound-literal lifetime semantics so the expired-pointer
  regression remains detectable.

Checks cover UUID lifetime, either pairing order, battery readings arriving before
identification, identity/power reads completing after disconnect, paced retries,
charging transitions, reconnect recovery, payload validation, and duplicate
suppression.

The Python runner creates temporary adapter headers and a host executable in a
system temporary directory. They are removed when the test finishes; they are
not firmware artifacts and are not written into the repository. The fixture
sources themselves are maintained test code and belong in version control.

### Power transitions — `test_power.py`

Compiles the production shutdown behavior for dongle, plain peripheral, and
display-peripheral roles. The display role includes the production VIK pin
handler; adapters in `fixtures/power/` check:

- Shutdown triggers on release, is queued outside BLE handling, and ignores duplicates.
- The real `SYS_INIT` level/priority enables VIK power before simulated DBI/panel
  initialization, without modifying the bus pins. Boot GPIO errors are propagated.
- The dongle stays powered and queue failures allow retrying.
- Idle preserves the normal display; automatic sleep selects the sleeping cat.
- Manual shutdown selects `BYE`, finishes the display callback before releasing
  control/output pins, then calls soft-off.
- Both Neko sprites fit their buffers with minimal and padded
  strides; rerendering clears stale artwork and leaves guard bytes intact.

Run just this suite with
`python3 -B -m unittest discover -s tests -p 'test_power.py'`.
It does not simulate the physical e-paper waveform or BLE command delivery; see
the [display guide](../boards/shields/mod_display_epaper_custom/README.md#verification)
for hardware checks.

## Hardware verification

These tests check state handling and memory safety, not real Bluetooth timing,
radio delivery, or display rendering. Follow the hardware verification steps in
the [split-sync README](../boards/shields/mod_split_state_sync/README.md) after
building and flashing firmware changes.
