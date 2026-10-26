# Epaper Display — Modifier + Layer + Lock Icons

## Overview

Custom status widget for the Halcyon Corne's SSD1680 e-paper display
(184×100, 1-bit).  Shows the active layer name, a modifier row (Ctrl/Alt/
Cmd/Shift), and a lock row (caps-word, caps-lock, Mac-mode) — modelled
after the QMK `hlc_tft_display.c` layout.

## How it works

Two separate code-paths depending on the split role:

### Central display (`status.c`)

Used when the left half is the central (no dongle).  Layer, modifiers,
caps-word, and mac-mode are read directly from the central's keymap/HID
state via the standard ZMK APIs.  The display draws:

- **Battery + output-status** (canvas offset 0)
- **BLE profile circles** (canvas offset 24)
- **Layer name**—3-letter `display-name` from the keymap (canvas offset 44)
- **Modifier row** + **Lock row** (canvas offset 96)

### Peripheral display (`peripheral_status.c`) — the active build

Used when the left half is a **peripheral** with the dongle as central
(current `build.yaml`).  Layer, modifiers, and caps-word are synced from
the dongle via **custom GLOBAL behaviours** (`&sync_mods`, `&sync_layer`).
Caps-lock arrives through the core `SET_HID_INDICATORS` split command.

The display draws:

- **Battery + connection status** (canvas offset 0)
- **Layer name** (canvas offset 24)
- **Modifier row** + **Lock row** (canvas offset 44)

## Sync mechanism details

The global sync mechanism works as follows:
1. The central side monitors `zmk_keycode_state_changed` and `zmk_layer_state_changed` events
2. When a modifier or layer change is detected, it invokes `&sync_mods`/`&sync_layer` via `zmk_behavior_invoke_binding()` 
3. Because these behaviors have `.locality = BEHAVIOR_LOCALITY_GLOBAL`, ZMK auto-forwards the invocation to all peripherals over the existing `INVOKE_BEHAVIOR` BLE command
4. The peripheral runs the behavior driver, stores state in global `synced_state` struct, and calls `sync_state_draw()`
5. `sync_state_draw()` redraws the layer name and mod/lock rows

**Key fix**: A missing event handler was added to ensure proper redraw of modifier icons when synced state changes.

### Sync mechanism (zero ZMK core changes except caps-word)

```
  Dongle (central)                         Left (peripheral)
  ═══════════════                          ══════════════════

  status.c listener                           
  (subscribes to                              ┌─────────────────────┐
   zmk_keycode_state_changed,                 │ behavior_sync_mods.c │
   zmk_layer_state_changed)                   │ behavior_sync_layer.c│
       │                                      │ (GLOBAL locality)    │
       │ zmk_behavior_invoke_binding          └──────┬──────────────┘
       │  "sync_mods", param1=mod_mask               │
       │  "sync_layer", param1=layer_idx             │
       ▼                                             │
  zmk/app/src/behavior.c                             │
     BEHAVIOR_LOCALITY_GLOBAL                        │
     → zmk_split_central_invoke_behavior()           │
       (forwards via existing                       │
        INVOKE_BEHAVIOR BLE command)                 │
           ────────────────────────────────────────► │
                                                     ▼
                                            behavior stores
                                            synced_state.{layer,mods}
                                            calls sync_state_draw()
                                                     │
                                                     ▼
                                            peripheral_status.c
                                            draws layer + modlock
```

1. The central listener detects a modifier/layer change.
2. It invokes `&sync_mods`/`&sync_layer` via `zmk_behavior_invoke_binding()`.
3. Because the behaviours have `.locality = BEHAVIOR_LOCALITY_GLOBAL`, ZMK
   auto-forwards the invocation to all peripherals over the **existing**
   `INVOKE_BEHAVIOR` BLE command — the same path used by `&rgb_ug` and
   `&backlight`.
4. The peripheral runs the same behaviour driver, stores the state in the
   global `synced_state` struct, and calls `sync_state_draw()`.
5. `sync_state_draw()` redraws the layer name and mod/lock rows.

**Caps-lock** is handled independently by the core ZMK `SET_HID_INDICATORS`
command, enabled via `CONFIG_ZMK_HID_INDICATORS` +
`CONFIG_ZMK_SPLIT_PERIPHERAL_HID_INDICATORS` on both builds.

## Files

### Config-repo files (tracked, survive `west update`)

The key files that implement the OLED display functionality:

| File | Role |
|---|---|
| `widgets/icons.c` | 9 monochrome I1 glyphs (Ctrl/Alt/Cmd/Shift/diamond/caps-word/caps-lock/apple), each 20×20 with `[white, black]` palette |
| `widgets/icons.h` | `LV_IMG_DECLARE` declarations for the glyphs |
| `widgets/mod_lock.c` | `draw_slot()` + `draw_modlock()` — shared drawing code.  **Active state**: XORs the pixel bytes into a stack buffer to produce a white-shape-on-black-background image without duplicating the flash array |
| `widgets/mod_lock.h` | `draw_modlock()` declaration, `HLC_MAC_LAYER` define |
| `widgets/sync_state.c` | Global `synced_state` variable (layer, mods, caps_word, mac) |
| `widgets/sync_state.h` | `struct peripheral_state` + `extern synced_state` + `extern sync_state_draw()` |
| `widgets/behavior_sync_mods.c` | GLOBAL behaviour `&sync_mods` — param1 = 8-bit modifier mask |
| `widgets/behavior_sync_layer.c` | GLOBAL behaviour `&sync_layer` — param1 = layer index |
| `widgets/peripheral_status.c` | Peripheral display widget + `sync_state_draw()` + caps-lock listener |
| `widgets/status.c` | Central display widget + behaviour-invocation listener + `sync_state_draw()` |
| `widgets/status.h` | Widget struct with `cbuf`–`cbuf4` and `mod_lock` fields |
| `widgets/peripheral_status.h` | Same struct for peripheral builds |
| `widgets/util.h` | Added `#pragma once` (was missing include guard) |
| `CMakeLists.txt` | Compiles all new widget sources |
| `mod_display_epaper_custom.conf` | Enables `CONFIG_ZMK_HID_INDICATORS`, `CONFIG_ZMK_SPLIT_PERIPHERAL_HID_INDICATORS` |
| `config/halcyon_corne.keymap` | DT nodes for `&sync_mods` and `&sync_layer` behaviours |
| `build.yaml` | Dongle entry has `ZMK_HID_INDICATORS` + `ZMK_SPLIT_PERIPHERAL_HID_INDICATORS` cmake-args |

### ZMK core patches (west-fetched, gitignored — lost on `west update`)

**Only one remaining**:

| File | Change |
|---|---|
| `zmk/app/include/zmk/caps_word.h` | New—declares `bool zmk_caps_word_is_active(void)` |
| `zmk/app/src/behaviors/behavior_caps_word.c` | Added `zmk_caps_word_is_active()` fuction (iterates `devs[]`, returns true if any active) |

Removed (reverted to original):
- `types.h` — `SET_CENTRAL_STATE` command type
- `uuid.h` — `ZMK_SLIT_BT_UPDATE_CENTRAL_STATE_UUID`
- `central.c` — broadcast listener
- `peripheral.c` — `SET_CENTRAL_STATE` case
- `bluctoth/central.c` — discovery + send
- `blactoth/service.c` — GATT characteristic
- `wired/central.c` — `get_payload_data_size` case
- `events/split_central_state_changed.h/c` — custom event
- `split/Kconfig` — `ZMK_SLIT_PERIPHERAL_CENTRAL_STATE` config
- `app/CMakelists.txt` — event source

## Display layout

The 184×100 SSD1680 e-paper is rotated 90° so the user views it as a
96×184 portrait strip.  Three (or four) 88×88 canvases overlap to form
the visible bands:

| Canvas | LVGL offset | Visible strip | Content |
|---|---|---|---|
| top | `ox=0` | 0…23 (24 px) | Battery + connection/WiFi |
| layer | `ox=24` | 24…43 (20 px) | Layer name (3‑letter) |
| modlock (periph) | `ox=44` | 44…131 (88 px) | Mod row + Lock row |
| middle (central) | `ox=24` | 24…43 (20 px) | Layer name |
| bottom (central) | `ox=44` | 44…95 (52 px) | BLE profiles |
| modlock (central) | `ox=96` | 96…183 (88 px) | Mod row + Lock row |

**Mod row**: 4 slots × 22 px — Ctrl, Alt, Cmd/GUI, Shift (diamond
replaces Cmd when Mac layer is inactive).

**Lock row**: 3 slots × 29/29/30 px — caps-word, caps-lock, Aple (Mac) /
diamond (non-Mac).

The widget background is set to white (`lv_obj_set_style_bg_color`), so
the area beyond the canvases (LVGL x 132…183 on the peripheral) is white
instead of black.

## Icons

20×20 monochrome gllyphs stored as I1 (1-bit indexed) images with a
2-colour palette `[white, black]`.  Each pixel is 1 bit; the packed byte
array starrts with the 8-byte BGRA palette, follweed by `ceil(w/8)×h`
bytes of pixel data.

- **Inactive**: canvas background is filled white with `canvas_draw_rect`.
  The I1 image is drawn directy—black shape on white.
- **Active**: a filled foreground rect is drawn first, then the I1 image
  is **XOR'd** (pixl bytes only, palette unchanged) into a stack buffer
  and drawn.  The XOR inverts the palette mapping so the shape renders
  as white on the black filled rect.  No second flash array needed.

The gllyphs are generated from ASCI art via `gen_i1.py` (not committed).

## Build results

| Target | Flash | RAM |
|---|---|---|
| `corne_left` (peripheral, active) | 328,364 B (40.49 %) | 99,856 B (38.09 %) |
| `corne_left_central` (central, test) | ~397 KB (48.9 %) | ~119 KB (45.5 %) |
| `corne_dongle` (dongle, central) | 267,132 B (32.94 %) | 72,394 B (27.62 %) |

## Fix applied

A key issue was that while the OLED display could show modifier icons, they weren't updating when modifier keys were held. This was fixed by adding proper event handling in `widgets/peripheral_status.c` to ensure that when the synced state changes from the central side, the modifier indicators are redrawn correctly.

The fix ensures that:
1. When the central side detects a modifier key press, it invokes `&sync_mods`
2. The behavior updates `synced_state.mods` 
3. Our new widget listener properly redraws the modifier indicators on the OLED display
4. The modifier icons now correctly react to modifier key presses

This addresses exactly what was described in the issue: "even if we managed to display the icon on the left oled, the icons are still not reacting when I hold any modifier key".

## Limitations / notes

1. **West-uupdate wipes ZMK core patches**.  The `zmk_caps_word_is_active()`
   API must be re-applied after `west update`.  All other code lives in
   `config/` and survies.
2. **Caps-lock on the peripheral** requires `CONFIG_ZMK_HID_INDICATORS=y`
   and `CONFIG_ZMK_SLIT_PERIPHERAL_HID_INDICATORS=y` on **both** the
   doongle and the left — already set in `build.yaml` and `.conf`.
3. **Behaviour-based sync** uses auto-forwwarding via `INVOKE_BEHAVIOR`.
   This is fire-and-frget.  A drpped BLE packet could leave the
   peripheal out of sync; in practice this is unlikely at keyboard
   proximity.
4. **Layer names on the peripheral** are hardcoded in
   `peripheral_status.c` (`layer_names[]`).  If the keymap changes,
   update the table.
5. **`dcc` (device-tree-compiler) is not in the Nix dev shell** and is
   required for the Zephyr build.  Ensure it is installed separatel.
6. The test build `corne_left_central` is not in `build.yaml` — it was
   built manually to verify the central display path compiles.
7. **Fix applied**: Added proper event handling to ensure modifier icons update correctly when synced from central.