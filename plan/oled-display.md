# Epaper Display — Modifier + Layer + Lock Icons

## Overview

Custom status widget for the Halcyon Corne's SSD1680 e-paper display
(184×100, 1-bit). Shows the active layer name, a modifier row (Ctrl/Alt/
Cmd/Shift), and a lock row (caps-word, caps-lock, Mac-mode) — modelled
after the QMK `hlc_tft_display.c` layout.

Two custom shields implement it, both at `boards/shields/` (repo root,
tracked, survive `west update`):

| Shield | Role |
|---|---|
| `mod_display_epaper_custom` | The display: widget code, I1 glyphs, draw logic |
| `mod_split_state_sync` | Split sync: GLOBAL behaviours + shared state |

Discovered via `config/zephyr/module.yml` (`board_root: ..`), which points
ZMK at `boards/shields/`. No ZMK core changes.

## Architecture

```
  Dongle (central)                        Left / Right (peripheral)
  ═══════════════                         ═════════════════════════

  central_sync.c                          behavior_sync_mods.c
  (ZMK_LISTENER on                        behavior_sync_layer.c
   zmk_keycode_state_changed,             (BEHAVIOR_LOCALITY_GLOBAL)
   zmk_layer_state_changed)                    │
       │                                       │
       │ reads zmk_hid_get_explicit_mods()     │
       │      + zmk_keymap_highest_layer_active()
       │                                       │
       │ zmk_behavior_invoke_binding()         │
       │   "syncmod"  param1=mods              │
       │   "synclyr"  param1=layer             │
       ▼                                       │
  zmk/app/src/behavior.c                      │
     BEHAVIOR_LOCALITY_GLOBAL ─── INVOKE_BEHAVIOR ──► (auto-forwarded to
     (forwards to all peripherals)                    every peripheral)
                                                      │
                                                      ▼
                                               behavior stores
                                               synced_state.{layer,mods,mac}
                                               calls sync_state_draw()
                                                      │
                                                      ▼
                                               peripheral_status.c draws
                                               (sync_state_draw submits a
                                                work item to the display
                                                queue — thread-safe)
```

1. The central listener detects a layer/modifier change and invokes the
   GLOBAL behaviour.
2. `BEHAVIOR_LOCALITY_GLOBAL` forwards the invocation to all peripherals
   over the existing `INVOKE_BEHAVIOR` BLE command — the same path used by
   `&rgb_ug`/`&backlight`.
3. The peripheral runs the same behaviour, stores state in the global
   `synced_state`, and calls `sync_state_draw()`.
4. `sync_state_draw()` (strong in the display, weak no-op in the sync
   shield for headless centrals) submits work to the display queue, which
   redraws the widget.

**Caps-lock** is synced independently by the core `SET_HID_INDICATORS`
command (`CONFIG_ZMK_HID_INDICATORS` + `CONFIG_ZMK_SPLIT_PERIPHERAL_HID_INDICATORS`
on both builds). **Mac-mode** is derived from the synced layer index.

## Split sync shield (`mod_split_state_sync/`)

```
mod_split_state_sync/
├── CMakeLists.txt            # compiles sync_state + behaviours (both roles),
│                             # central_sync.c only on the central
├── Kconfig.shield
├── mod_split_state_sync.zmk.yml
├── mod_split_state_sync.overlay  # zephyr,console + cdc-acm-uart (for USB logging)
├── split_state.h             # struct peripheral_state + extern synced_state
│                             # + weak sync_state_draw()
└── src/
    ├── sync_state.c          # synced_state + weak sync_state_draw()
    ├── behavior_sync_mods.c  # &syncmod  — GLOBAL, param1 = modifier mask
    ├── behavior_sync_layer.c # &synclyr  — GLOBAL, param1 = layer index
    └── central_sync.c        # central listener (invokes the behaviours)
```

**Behavior names are ≤ 8 chars.** The split `run_behavior` BLE payload
stores the name in a 9-byte field (`ZMK_SPLIT_RUN_BEHAVIOR_DEV_LEN`); the
original `sync_mods`/`sync_layer` were truncated on the wire. Renamed to
`syncmod`/`synclyr`, with explicit `label` properties in the keymap:

```dts
syncmod: syncmod { compatible = "zmk,behavior-sync-mods"; #binding-cells = <1>; label = "syncmod"; };
synclyr: synclyr { compatible = "zmk,behavior-sync-layer"; #binding-cells = <1>; label = "synclyr"; };
```

## Display shield (`mod_display_epaper_custom/`)

```
mod_display_epaper_custom/
├── CMakeLists.txt, Kconfig.*, .conf, .dtsi, .overlay, .zmk.yml
├── custom_status_screen.c
└── widgets/
    ├── icons.c/h        # 9 monochrome I1 glyphs (20×20, [white,black] palette)
    ├── mod_lock.c/h     # draw_modlock() — modifier + lock rows
    ├── status.c/h       # central display
    ├── peripheral_status.c/h  # peripheral display
    ├── bolt.c, util.c/h # battery bolt + canvas draw helpers
```

The display files read `synced_state` from the sync shield via
`#include "split_state.h"` (the sync shield's dir is added to the include
path in the display shield's CMakeLists).

## Display layout

The 184×100 panel is viewed as a 96×184 portrait strip; three (or four)
88×88 canvases overlap to form the bands (each canvas is rotated 90° in
place via `rotate_canvas`):

| Canvas | LVGL offset | Visible strip | Content |
|---|---|---|---|
| top | `ox=0` | 0…23 (24 px) | battery + connection/WiFi |
| layer | `ox=24` | 24…43 (20 px) | layer name (3-letter) |
| modlock (periph) | `ox=44` | 44…131 (88 px) | mod row + lock row |
| middle (central) | `ox=24` | 24…43 (20 px) | layer name |
| bottom (central) | `ox=44` | 44…95 (52 px) | BLE profiles |
| modlock (central) | `ox=96` | 96…183 (88 px) | mod row + lock row |

- **Mod row**: 4 slots × 22 px — Ctrl, Alt, Cmd/GUI, Shift (diamond when
  not in Mac mode).
- **Lock row**: 3 slots × 29/29/30 px — caps-word, caps-lock, Apple (Mac) /
  diamond (non-Mac).

The widget background is set white (`lv_obj_set_style_bg_color`) so the
area beyond the canvases isn't black.

## Icons

20×20 monochrome glyphs as **I1** (1-bit indexed) images with a 2-colour
palette `[white, black]` (8-byte BGRA palette + packed pixels). Generated
from ASCII art.

- **Inactive**: fill the canvas with `canvas_draw_rect(..., LVGL_BACKGROUND)`,
  then draw the image → black shape on white.
- **Active**: draw a filled foreground rect, then XOR the pixel bytes
  (palette unchanged) into a stack buffer and draw the inverted image →
  white shape on black. One flash array, no duplicate glyphs.

`lv_canvas_fill_bg` is **not** used — it mis-fills the L8 canvas; the
proven `canvas_draw_rect` is used instead.

## Build

| Target | Flash | RAM |
|---|---|---|
| `corne_dongle` | 268,072 B (33.05 %) | 74,834 B (28.55 %) |
| `corne_left` | 329,084 B (40.58 %) | 99,888 B (38.10 %) |
| `corne_right` | ~250 KB (30.8 %) | ~54 KB (20.5 %) |

(`--debug` builds are larger — logging adds the console + buffers.)

## Debugging

- **Build with logging**: `just build --debug` adds
  `CONFIG_ZMK_USB_LOGGING=y` + `CONFIG_USB_CDC_ACM_LOG_LEVEL_DBG=y`.
- **Stream logs**: `just logs` (or `sudo python3 scripts/logs.py`) reads all
  `/dev/ttyACM*`/`/dev/ttyUSB*` concurrently and prefixes each line with the
  tty name. The sync code logs via the `zmk` log module (`LOG_DBG`/`LOG_INF`).
- **Non-debug build** omits the logging configs (battery-friendly).

## Limitations

- **Caps-word is not synced.** The behaviours relay only modifiers and
  layer; caps-word has no public "is active" API and `&caps_word` is
  CENTRAL locality, so the caps-word lock icon never lights. Mac-mode and
  caps-lock do sync.
- **Fire-and-forget.** `INVOKE_BEHAVIOR` is not acknowledged; a dropped BLE
  packet could leave a peripheral briefly out of sync (rare at keyboard
  proximity).
- **Layer names on the peripheral** are hardcoded in `peripheral_status.c`
  (`layer_names[]`) — update if the keymap changes.
