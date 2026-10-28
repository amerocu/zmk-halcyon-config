# AGENTS.md

## What this repo is
- A fork of splitkb's Halcyon ZMK **config** repo. Hardware/keyboard definitions (boards, shields, drivers) live in the external `zmk-halcyon-module`, pulled by `west`; this repo tracks the keymap, keymap-editor JSON, `build.yaml`, and two custom shields.
- CI (`.github/workflows/build.yml`) delegates to the shared `zmkfirmware/zmk/.github/workflows/build-user-config.yml@main`; each `build.yaml` entry becomes a firmware artifact.
- Active target: Corne dongle + left (custom epaper display) + right (Cirque) — see `build.yaml`.

## Build / flash (local)
- Enter the Nix dev shell first: `nix develop` (provides cmake, ninja, arm gcc, protobuf, python/west). `just` is **not** in the dev shell; `dtc` (device-tree-compiler) is not either.
- One-time: `just setup` (runs `west init -l config` if needed, then `west update`). Build: `just build` (all entries) or `just build <filter>` (substring of the artifact name). Output: `firmware/<artifact-name>.uf2`.
- `just build --debug` builds with USB logging enabled (`CONFIG_ZMK_USB_LOGGING=y` + debug log level). Stream the logs with `just logs` (reads all `/dev/ttyACM*`/`/dev/ttyUSB*`, prefixes each line with the tty name; needs sudo).
- `just flash [filter]` flashes over UF2 mass-storage; **entries without `artifact-name` are silently skipped**. `just list-targets` prints resolved names. `just clean` removes `build/`+`firmware/`; `just distclean` removes the whole west workspace.

## Where customization lives
- `config/halcyon_corne.keymap` — active keymap (layers QWE/Sym/Nav/Fun/Adj/Mac/SyM, combos, `&studio_unlock`, `&rgb_ug`). `config/*.json` are keymap-editor layouts for each keyboard.
- `boards/shields/mod_display_epaper_custom/` — the custom epaper status widget (I1 glyphs, modifier/lock rows). Discovered via `config/zephyr/module.yml` (`board_root: ..`), which points at `boards/shields/` in the repo root.
- `boards/shields/mod_split_state_sync/` — the split-state sync module. Two GLOBAL behaviours (`&syncmod`, `&synclyr`) forward the central's modifier/layer state to every half over the existing `INVOKE_BEHAVIOR` BLE command (no ZMK core changes). Its `.overlay` also adds a `zephyr,console` CDC-ACM node so `just build --debug` logging works on these boards.
- `scripts/logs.py` — the multi-device log streamer used by `just logs`.
- `hlc_tft_display.c` at repo root is **untracked and referenced by no build**; it targets the separate `mod_display_tft` (ST7789 135x240) shield, not the active epaper build.
- **ZMK core patches: none.** The split sync and caps-word display are done without touching `zmk/` (west-fetched, gitignored). If a prior `west update` left a `zmk_caps_word_is_active()` patch, it's no longer needed — re-apply or discard as appropriate.

## Quirks
- The real west workspace is at the repo root (`.west/`, `zephyr/`, `zmk/`, `zmk-halcyon-module/`), all gitignored. `.zmk/` is a stale ZMK CLI cache — ignore it.
- `config/zephyr/module.yml` sets `board_root: ..`, which is how `boards/shields/*` gets discovered (the old `config/boards/` path is deprecated and must not be used).
- **Behavior names must be ≤ 8 chars.** The split `run_behavior` BLE payload stores the name in a 9-byte field; `syncmod`/`synclyr` fit, `sync_mods`/`sync_layer` would be truncated on the wire.
- `build.yaml` shield order matters: `<keyboard>_<side> <battery> mod_*`. Dongle = `<keyboard>_dongle mod_split_state_sync`. Left/right also append `mod_split_state_sync`. Non-dongle central (left) adds `cmake-args: -DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=n`; Studio builds add `-DCONFIG_ZMK_STUDIO=y` plus `snippet: studio-rpc-usb-uart` on the central.
- Build artifacts (`build/`, `firmware/`) are gitignored; do not commit them.

## Known limitation
- **Caps-word is not synced.** The `&syncmod`/`&synclyr` behaviours only relay modifiers and layer; the caps-word lock icon stays off because caps-word has no public "is active" API and `&caps_word` has CENTRAL locality. Mac-mode and caps-lock (via core `SET_HID_INDICATORS`) do sync.
