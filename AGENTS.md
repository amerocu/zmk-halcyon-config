# AGENTS.md

## What this repo is
- A fork of splitkb's Halcyon ZMK **config** repo. Hardware/keyboard definitions (boards, shields, drivers) live in the external `zmk-halcyon-module`, pulled by `west`; this repo only tracks keymaps, keymap-editor JSON, `build.yaml`, and one custom shield.
- CI (`.github/workflows/build.yml`) delegates to the shared `zmkfirmware/zmk/.github/workflows/build-user-config.yml@main`; each `build.yaml` entry becomes a firmware artifact.
- Active target: Corne dongle + left (custom epaper display) + right (Cirque) — see `build.yaml`.

## Build / flash (local)
- Enter the Nix dev shell first: `nix develop` (provides cmake, ninja, arm gcc, protobuf, python/west). `just` is **not** in the dev shell; `dtc` (device-tree-compiler) is not either.
- One-time: `just setup` (runs `west init -l config` if needed, then `west update`). Build: `just build` (all entries) or `just build <filter>` (substring of the artifact name). Output: `firmware/<artifact-name>.uf2`.
- `just flash [filter]` flashes over UF2 mass-storage; **entries without `artifact-name` are silently skipped**. `just list-targets` prints resolved names. `just clean` removes `build/`+`firmware/`; `just distclean` removes the whole west workspace.

## Where customization lives
- `config/halcyon_corne.keymap` — active keymap (layers QWE/Sym/Nav/Fun/Adj/Mac/SyM, combos, `&studio_unlock`, `&rgb_ug`). `config/*.json` are keymap-editor layouts for each keyboard.
- `config/boards/shields/mod_display_epaper_custom/` — repo-local copy of the module's `mod_display_epaper` shield (referenced as `mod_display_epaper_custom` in `build.yaml`). It drops the built-in art image and customizes the status widget. **Edit this copy, not `zmk-halcyon-module/boards/shields/mod_display_epaper/` (west-fetched, gitignored, overwritten on update).**
- `hlc_tft_display.c` at repo root is **untracked and referenced by no build**; it targets the separate `mod_display_tft` (ST7789 135x240) shield, not the active epaper build.
- **ZMK core patches** (in `zmk/`, west-fetched, gitignored; lost on `west update`):
  - `zmk/app/include/zmk/caps_word.h` + `zmk/app/src/behaviors/behavior_caps_word.c` — public `zmk_caps_word_is_active()` API.
  - Split central-state sync: `zmk/app/include/zmk/split/transport/types.h` (SET_CENTRAL_STATE command), `zmk/app/include/zmk/events/split_central_state_changed.h` + `zmk/app/src/events/split_central_state_changed.c` (new event), `zmk/app/src/split/central.c` (broadcast listener), `zmk/app/src/split/peripheral.c` (receiver), `zmk/app/src/split/bluetooth/service.c` + `zmk/app/src/split/bluetooth/central.c` (BLE GATT transport), `zmk/app/src/split/wired/central.c` (wired transport), `zmk/app/include/zmk/split/bluetooth/uuid.h` (GATT UUID), `zmk/app/src/split/Kconfig` (CONFIG_ZMK_SPLIT_PERIPHERAL_CENTRAL_STATE), `zmk/app/CMakeLists.txt` (event source).

## Quirks
- The real west workspace is at the repo root (`.west/`, `zephyr/`, `zmk/`, `zmk-halcyon-module/`), all gitignored. `.zmk/` is a stale ZMK CLI cache — ignore it.
- `zephyr/module.yml` sets `board_root: .`, which is how `config/boards/shields/*` gets discovered.
- `build.yaml` shield order matters: `<keyboard>_<side> <battery> mod_*`. Dongle = `<keyboard>_dongle [mod_cirque_central]`. Non-dongle central (left) adds `cmake-args: -DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=n`; Studio builds add `-DCONFIG_ZMK_STUDIO=y` plus `snippet: studio-rpc-usb-uart` on the central.
- Build artifacts (`build/`, `firmware/`) are gitignored; do not commit them.
