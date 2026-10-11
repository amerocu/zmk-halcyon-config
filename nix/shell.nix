{
  mkShell,
  cmake,
  gcc-arm-embedded,
  ninja,
  protobuf,
  python3,
}: let
  pythonEnv = python3.withPackages (ps: [
    # From https://github.com/zmkfirmware/zephyr/blob/HEAD/scripts/requirements-base.txt
    ps.west
    ps.pyelftools
    ps.pyyaml
    ps.pykwalify
    ps.canopen
    ps.packaging
    ps.progress
    ps.psutil
    ps.pylink-square
    ps.pyserial
    ps.requests
    ps.anytree
    ps.intelhex
    ps.patool
    ps.semver
    ps.tqdm
    # From https://github.com/zmkfirmware/zmk/blob/HEAD/app/scripts/requirements.txt
    ps.remarshal
    ps.jsonschema
    # For ZMK Studio builds
    ps.protobuf
    ps.grpcio-tools
  ]);
in
  mkShell {
    packages = [
      cmake
      ninja
      pythonEnv
      gcc-arm-embedded
      protobuf
    ];

    env = {
      # The nixpkgs `west` wrapper prepends the bare python3 (no packages) to PATH, so
      # tools using `#!/usr/bin/env python3` (e.g. nanopb's protoc plugin) would miss
      # protobuf & co. Expose the env's site-packages to any interpreter.
      PYTHONPATH = "${pythonEnv}/${pythonEnv.sitePackages}";
      ZEPHYR_TOOLCHAIN_VARIANT = "gnuarmemb";
      GNUARMEMB_TOOLCHAIN_PATH = gcc-arm-embedded;
    };
  }
