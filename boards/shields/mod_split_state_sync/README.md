# Split state sync

The dongle forwards layer/modifier state and the right half's battery to the
left display using ZMK's existing `INVOKE_BEHAVIOR` split command. Device
identification uses Zephyr's standard BLE Device Information Service (DIS).

## Device identity

Each `build.yaml` target selects exactly one Kconfig choice:

```ini
CONFIG_SPLIT_STATE_SYNC_DEVICE_DONGLE=y
# or CONFIG_SPLIT_STATE_SYNC_DEVICE_LEFT=y
# or CONFIG_SPLIT_STATE_SYNC_DEVICE_RIGHT=y
```

`CONFIG_SPLIT_STATE_SYNC_DEVICE_NAME` derives the corresponding string
(`dongle`, `left`, or `right`). It is exposed as the DIS serial-number value.
These are logical identities within this keyboard, not hardware serial numbers.

Code can use `IS_ENABLED(CONFIG_SPLIT_STATE_SYNC_DEVICE_LEFT)` for conditional
compilation, the same option in CMake, or `sync_device_local_id()` from
`device_identity.h` for runtime decisions.

On each peripheral connection, the dongle reads the DIS serial number and maps
the ZMK source/pairing slot to the logical identity. Pairing order is irrelevant.
Unknown identities and duplicate connected identities are not used for routing.
Failed identity reads retry with a delay. An in-flight read retains its connection
reference until completion so disconnects cannot reuse its read parameters.

## Battery forwarding

The dongle identity enables ZMK's existing
`CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING` option by default. Battery
readings are cached per connection, including readings received before identity
discovery finishes. Bluetooth connection callbacks track availability separately
from battery percentage: ZMK's synthetic `0%` disconnect event is not treated as
a connected empty battery.

`syncbat` sends the right battery to the identified left half:

- `param1`: logical device ID (`SYNC_DEVICE_RIGHT`).
- `param2`: percentage in bits 0–7, connected in bit 8, valid reading in bit 9,
  charging/USB-powered in bit 10.

The right firmware also exposes a one-byte USB-power characteristic in a custom
BLE battery-status service. Once identified, the dongle reads it at most once per
second, independently of battery percentage. This uses the same USB-powered
signal as the local battery's charging bolt; it is not a charger-IC measurement.
Battery-level updates preserve the last power reading. Disconnects clear it.

Snapshots are sent when state or connections change and every five seconds to
recover from commands sent before split characteristic discovery completes.
These snapshots forward cached status. The separate power reads do not measure
the battery or change the halves' normal battery measurement interval.

The left rejects invalid payloads and packets received while disconnected.
It emits `zmk_sync_battery_state_changed` only if percentage, availability,
validity, or charging state changes. The display also compares its cached state
before redrawing the status section. Repeated snapshots do not redraw the display
or its layer/modifier sections.

The display shows two unlabelled batteries: left first, right second. The right
battery shows `?` whenever disconnected/unavailable or awaiting a reading.
A connected `0%` is an ordinary empty battery. Both icons show the charging bolt
when their half is USB-powered.
Loss of the left-to-dongle link invalidates the right battery locally; a new
snapshot restores it after reconnect.

## Build and hardware verification

Build all three targets with `just build` inside `nix develop`, then flash all
three using `just flash`. Both halves need the new identity firmware. Existing
bonds can be retained.

Check these transitions on hardware:

1. Both halves connect: the first battery is local, the second is the right's.
2. Turn off the right: its battery shows `?` after BLE detects link loss.
3. Reconnect the right: its initial battery read restores the indicator.
4. Restart the left: the current right reading returns without a keypress.
5. Unplug the dongle: the remote battery shows `?` locally.
6. Leave levels unchanged: periodic snapshots cause no status redraw.
7. Plug/unplug right USB while BLE stays connected: the right bolt changes even
   if its percentage stays the same.

For discovery diagnostics, use `just build --debug`, flash, and run `just logs`.
The dongle logs `source N identified as left/right` after reading each identity.

The `split sync:` diagnostics show the local device/serial at boot, identity
read results (including ATT errors), battery events, and queued/received `syncbat`
commands. At debug level, the dongle prints a routing/battery status summary every
five seconds, even while waiting for an identity; source `-1` means that identity
is not yet routable. The left logs changes and battery-section redraws separately,
so repeated snapshot receipts can be distinguished from actual display updates.

Run `python3 -B -m unittest discover -s tests -p 'test_split_battery.py'` for host
regression checks (requires `cc` with AddressSanitizer). These exercise production
C code with BLE/kernel adapters, including the lifetime of the identity UUID,
pairing order, disconnects during identity/power reads, paced retries, charging
transitions, reconnects, payload validation, and duplicate suppression.
They do not replace checking BLE discovery and delivery on hardware.
See the [test guide](../../../tests/README.md) for the harness layout, requirements,
and instructions for running all host-side suites.
