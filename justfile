# Local ZMK build driven by build.yaml.
# Place this at the root of your zmk-config repo (next to `config/` and `build.yaml`).
#
# Prerequisites (provided by the Nix dev shell, see nix/shell.nix):
#   - cmake, ninja, gcc-arm-embedded, protobuf
#   - Python deps (west, protobuf, grpcio-tools, etc.)
# Also needed (not in the dev shell): just, git, dtc (device-tree-compiler).
#   - Zephyr SDK (set ZEPHYR_SDK_INSTALL_DIR or install to a default location);
#     use the version your ZMK release requires.

set shell := ["bash", "-euo", "pipefail", "-c"]

root   := justfile_directory()
config := root / "config"

# Point CMake at the Zephyr checkout that `west update` creates next to config/
export ZEPHYR_BASE := root / "zephyr"

default:
    @just --list

# One-time setup: fetch ZMK and Zephyr (Python deps come from the Nix dev shell)
setup:
    [ -d "{{root}}/.west" ] || west init -l "{{config}}"
    west update
    west zephyr-export
    @echo "Setup done. Run: just build"

# Pull the ZMK/Zephyr/module versions pinned in config/west.yml
update:
    west update

# Build every entry in build.yaml. Optional filter: only entries whose name contains it,
# e.g. `just build left`. Output .uf2 files go to ./firmware
build filter="":
    #!/usr/bin/env python
    import shlex, shutil, subprocess, sys
    from pathlib import Path
    import yaml

    root = Path("{{root}}")
    flt = "{{filter}}"

    for d in (".west", "zmk", "zephyr/share/zephyr-package"):
        if not (root / d).exists():
            sys.exit(f"Workspace incomplete (missing {d}). Run: just setup")

    entries = (yaml.safe_load((root / "build.yaml").read_text()) or {}).get("include", [])
    out = root / "firmware"
    out.mkdir(exist_ok=True)

    built = 0
    for e in entries:
        board, shield = e["board"], e.get("shield", "")
        board_base = board.split("//")[0]
        # Same naming idea as the GitHub Action: artifact-name, else <first shield>-<board>
        name = e.get("artifact-name") or f"{(shield.split() or ['noshield'])[0]}-{board_base}"
        if flt and flt not in name:
            continue

        cmd = ["west", "build", "-p", "auto", "-s", "zmk/app",
               "-d", f"build/{name}", "-b", board]
        if e.get("snippet"):
            cmd += ["-S", e["snippet"]]
        cmd += ["--", f"-DZMK_CONFIG={root / 'config'}"]
        if shield:
            cmd.append(f"-DSHIELD={shield}")
        cmd += shlex.split(e.get("cmake-args", ""))

        print(f"\n=== {name} ===", flush=True)
        subprocess.run(cmd, cwd=root, check=True)

        built_dir = root / "build" / name / "zephyr"
        src = next((p for p in (built_dir / "zmk.uf2", built_dir / "zmk.bin") if p.exists()), None)
        if src is None:
            sys.exit(f"No firmware found in {built_dir}")
        shutil.copy(src, out / f"{name}{src.suffix}")
        built += 1

    if built == 0:
        sys.exit("No build.yaml entries matched" + (f" '{flt}'" if flt else ""))
    print(f"\nDone: {built} firmware file(s) in {out}")

# List the targets that `just build` would produce
list-targets:
    #!/usr/bin/env python
    import yaml
    from pathlib import Path
    for e in yaml.safe_load(Path("{{root}}/build.yaml").read_text())["include"]:
        shield = e.get("shield", "")
        board = e["board"].split("//")[0]
        print(e.get("artifact-name") or f"{(shield.split() or ['noshield'])[0]}-{board}")

# Flash every entry in build.yaml, in order. Optional filter: only entries whose
# name contains it, e.g. `just flash left`. Double-tap reset each device when prompted.
flash filter="":
    #!/usr/bin/env python
    import subprocess, time, sys
    from pathlib import Path
    import yaml

    root = Path("{{root}}")
    flt = "{{filter}}"

    entries = (yaml.safe_load((root / "build.yaml").read_text()) or {}).get("include", [])

    def find_device():
        out = subprocess.run(["lsblk", "-Sno", "path,model"], capture_output=True, text=True).stdout
        for line in out.splitlines():
            if "nRF UF2" in line:
                return line.split()[0]
        return None

    def find_mountpoint(device):
        out = subprocess.run(["findmnt", device, "-no", "target"], capture_output=True, text=True).stdout.strip()
        return out or None

    flashed = 0
    for e in entries:
        name = e.get("artifact-name")
        if not name or (flt and flt not in name):
            continue

        uf2 = root / "firmware" / f"{name}.uf2"
        if not uf2.exists():
            sys.exit(f"Missing {uf2}. Run: just build")

        print(f"\n=== Flash {name} ===", flush=True)
        print("Double-tap reset and plug in the device...", flush=True)

        device = None
        while not device:
            device = find_device()
            if not device:
                time.sleep(1)
        print(f"Found device at {device}", flush=True)

        mountpoint = None
        while not mountpoint:
            subprocess.run(["udisksctl", "mount", "--block-device", device], capture_output=True)
            mountpoint = find_mountpoint(device)
            if not mountpoint:
                time.sleep(1)
        print(f"Found mountpoint at {mountpoint}", flush=True)

        subprocess.run(["cp", str(uf2), mountpoint], check=True)
        print(f"Flashed {name}", flush=True)
        flashed += 1

        time.sleep(1)
        while find_mountpoint(device):
            time.sleep(1)

    if flashed == 0:
        sys.exit("No build.yaml entries matched" + (f" '{flt}'" if flt else ""))
    print(f"\nDone: flashed {flashed} device(s)")

# Remove build output (keeps the west workspace)
clean:
    rm -rf build firmware

# Remove everything fetched by setup (you'll need to run `just setup` again)
distclean: clean
    rm -rf .west zmk zephyr modules zmk-modules bootloader tools