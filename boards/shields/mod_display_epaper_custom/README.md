# Custom epaper status screen

## Layout

The upright content area is 88 pixels wide and 184 pixels tall. `widgets/util.h`
defines section heights and offsets in upright coordinates. Rotation is internal:
each displayed canvas is a native `height × 88` rectangle, while its drawing
callback receives an `88 × height` scratch canvas.

The active peripheral layout is:

| Section | Upright vertical range |
| --- | --- |
| `SECTION_STATUS` (battery/connection) | 0–23 |
| `SECTION_LAYER` | 24–43 |
| Free content area | **44–163** |
| `SECTION_MODIFIERS` | **164–183** |

The optional central screen also registers `SECTION_PROFILES` at 44–75.
`MODIFIER_OFFSET` is calculated from the screen length, independently of the
other sections. No canvas covers the free content area on the peripheral.

## Full redraw versus section redraw

All drawing and state changes must run on ZMK's display queue.

- `redraw_section(&widget->sections[SECTION_LAYER])` redraws just that piece
  from its cached state. A completed hidden scratch image is rotated into the
  fixed visible buffer and that section alone is invalidated.
- `zmk_widget_status_redraw(widget)` redraws every registered section and requests
  an explicit full-screen refresh. Initialization uses this path after loading
  the initial state.

Routine callbacks compare their new state with the cache before calling
`redraw_section()`. Modifier events do not redraw the layer, layer events do not
redraw modifiers, and repeated unchanged state does not invalidate either.
Registered sections have named references rather than LVGL child indices.

## Why presentation is custom

Zephyr's standard monochrome flush callback enables display blanking when LVGL
flushes more than one region in a frame. The SSD16xx driver switches to its full
refresh profile when blanking is enabled. Disjoint section updates would therefore
flash the whole panel, even with `CONFIG_LV_Z_FULL_REFRESH=n`.

`widgets/refresh.c` keeps LVGL's monochrome area rounding and converts its I1
pixels to the panel's monochrome layout, but presents ordinary windows as
independent partial updates. Multiple dirty sections can produce several partial
updates; they do not implicitly select the full waveform. Only
`epaper_refresh_full()` enables blanking until the last window is written.

With a debug build, `just logs` shows the native window coordinates and whether
the request is partial or full. The mono area-rounder can expand a window to
byte boundaries (for example, modifiers at 164–183 are flushed at 160–183).

## Adding a section

1. Choose an unused interval inside `[CONTENT_OFFSET, CONTENT_END)` and define
   its offset/height in `widgets/util.h`. Check that it does not overlap neighbors.
2. Add a named ID before `SECTION_COUNT`, and a buffer sized with
   `SECTION_BUF_SIZE(height)` to the appropriate widget structure. Include the
   height in `MAX_SECTION_HEIGHT` if it exceeds the current maximum.
3. Write a `section_draw_cb`: `(lv_obj_t *canvas, const void *context)`. Call
   `canvas = begin_canvas(canvas)`, draw within the section's upright bounds,
   then call `rotate_canvas(canvas)` to publish it. Use cached state in `context`.
4. Register it with `init_section()` in the widget's initialization. The full
   redraw path automatically includes registered sections.
5. Update its cached state and call `redraw_section()` only when that state changes.

Keep the modifier offset bottom-anchored. Adding a new section does not require
changing the modifier renderer or refresh implementation.

## Verification

`python3 scripts/test_epaper_refresh.py` exercises the actual custom flush callback
against the fetched SSD16xx write/blanking functions with fake hardware endpoints.
It checks individual and simultaneous section updates, an explicit full refresh,
return to partial refresh, and monochrome conversion with padded source strides.

Build the active display firmware with `nix develop -c just build left`.
