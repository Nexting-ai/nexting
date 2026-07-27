#!/usr/bin/env bash
set -euo pipefail

devices_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
workspace_dir="$(dirname "$devices_dir")"
board_alias="xiao-nrf52840-sense"
should_build=0
dry_run=0
install_sdk=0

usage() {
  sed -n '2,31p' "$0" | sed -n 's/^# \{0,1\}//p'
}

# bootstrap-zephyr.sh [options]
#
# Prepare the pinned Nexting Devices Zephyr workspace and optionally build.
#
# Options:
#   --board ALIAS    xiao-nrf52840-sense (default), nrf52840-dk,
#                    xiao-esp32c3, or xiao-esp32s3
#   --build          Build the selected reference firmware after bootstrap
#   --install-sdk    Install Zephyr SDK 0.17.4 for the selected architecture
#   --dry-run        Print the resolved setup and build without changing files
#   -h, --help       Show this help
#
# Run this script from any directory. It creates .west and a Python virtual
# environment beside devices/, leaving the checked-out source tree unchanged.

die() {
  printf 'ERROR: %s\n' "$*" >&2
  exit 64
}

while (($#)); do
  case "$1" in
    --board)
      (($# >= 2)) || die "--board requires an alias"
      board_alias="$2"
      shift 2
      ;;
    --build)
      should_build=1
      shift
      ;;
    --install-sdk)
      install_sdk=1
      shift
      ;;
    --dry-run)
      dry_run=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      die "unknown option: $1"
      ;;
  esac
done

case "$board_alias" in
  xiao-nrf52840-sense)
    board="xiao_ble/nrf52840/sense"
    toolchain="arm-zephyr-eabi"
    image="zephyr.uf2"
    ;;
  nrf52840-dk)
    board="nrf52840dk/nrf52840"
    toolchain="arm-zephyr-eabi"
    image="zephyr.hex"
    ;;
  xiao-esp32c3)
    board="xiao_esp32c3/esp32c3"
    toolchain="riscv64-zephyr-elf"
    image="zephyr.bin"
    ;;
  xiao-esp32s3)
    board="xiao_esp32s3/esp32s3/procpu"
    toolchain="xtensa-espressif_esp32s3_zephyr-elf"
    image="zephyr.bin"
    ;;
  *)
    die "unsupported board '$board_alias'. Use xiao-nrf52840-sense, nrf52840-dk, xiao-esp32c3, or xiao-esp32s3."
    ;;
esac

build_dir="$devices_dir/build/$board_alias"
venv_dir="$workspace_dir/.nexting-zephyr-venv"
west_bin="$venv_dir/bin/west"

printf 'Nexting Devices Zephyr bootstrap\n'
printf 'West version: 1.5.0\n'
printf 'Zephyr revision: v4.3.0\n'
printf 'Zephyr SDK: 0.17.4\n'
printf 'Board: %s\n' "$board"
printf 'Workspace: %s\n' "$workspace_dir"
printf 'Build directory: %s\n' "$build_dir"
printf 'Build option: -DEXTRA_CONF_FILE=debug-test-device.conf\n'
printf 'Expected image: %s/zephyr/%s\n' "$build_dir" "$image"

if ((dry_run)); then
  printf 'DRY RUN: no files changed.\n'
  exit 0
fi

command -v python3 >/dev/null 2>&1 ||
  die "python3 is required. Install Python 3.11+ and rerun this command."

if [[ ! -x "$west_bin" ]]; then
  printf '\n[1/5] Creating isolated Python environment...\n'
  python3 -m venv "$venv_dir"
  "$venv_dir/bin/python" -m pip install --upgrade pip
  "$venv_dir/bin/python" -m pip install "west==1.5.0"
fi

cd "$workspace_dir"

if [[ ! -d "$workspace_dir/.west" ]]; then
  printf '\n[2/5] Initializing the local manifest workspace...\n'
  "$west_bin" init -l "$devices_dir"
else
  printf '\n[2/5] Reusing %s/.west\n' "$workspace_dir"
fi

printf '\n[3/5] Fetching the pinned Zephyr modules...\n'
"$west_bin" update
"$west_bin" zephyr-export
"$venv_dir/bin/python" -m pip install -r "$workspace_dir/zephyr/scripts/requirements.txt"

if ((install_sdk)); then
  printf '\n[4/5] Installing Zephyr SDK 0.17.4 (%s)...\n' "$toolchain"
  "$west_bin" sdk install --version 0.17.4 --toolchains "$toolchain"
else
  printf '\n[4/5] Keeping the installed Zephyr SDK.\n'
  printf '      If the build reports a missing toolchain, rerun with --install-sdk.\n'
fi

if ((should_build)); then
  printf '\n[5/5] Building %s...\n' "$board"
  extra_args=(-DEXTRA_CONF_FILE=debug-test-device.conf)
  if [[ "$board_alias" == xiao-esp32* ]]; then
    "$west_bin" blobs fetch hal_espressif
  fi
  "$west_bin" build -p always -b "$board" "$devices_dir/firmware/zephyr" \
    -d "$build_dir" -- "${extra_args[@]}"
  [[ -f "$build_dir/zephyr/$image" ]] ||
    die "build completed without expected image $build_dir/zephyr/$image"
  printf '\nPASS firmware=%s/zephyr/%s\n' "$build_dir" "$image"
else
  printf '\n[5/5] Bootstrap complete. Add --build to compile firmware.\n'
fi
