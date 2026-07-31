#!/bin/sh
set -eu

usage() {
  cat <<'EOF'
Usage:
  flash-multipad.sh backup --port /dev/cu.usbmodem... --out backup.bin
  flash-multipad.sh write --port /dev/cu.usbmodem... --hex leden.hex \
      --confirm --allow-write

The write path is intentionally fail-closed. It requires both flags, an
explicit serial port, and an explicit Intel HEX file. This script does not
discover a BOOT/RESET sequence or claim that ordinary USB CDC is a bootloader.
EOF
}

action=${1:-}
shift || true
port=
hex=
out=
confirm=false
allow_write=false

while [ "$#" -gt 0 ]; do
  case "$1" in
    --port) port=${2:?missing value for --port}; shift 2 ;;
    --hex) hex=${2:?missing value for --hex}; shift 2 ;;
    --out) out=${2:?missing value for --out}; shift 2 ;;
    --confirm) confirm=true; shift ;;
    --allow-write) allow_write=true; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

[ -n "$port" ] || { echo "--port is required" >&2; exit 2; }
command -v stm32flash >/dev/null 2>&1 || {
  echo "stm32flash is required (install it before using this script)" >&2
  exit 1
}

case "$action" in
  backup)
    [ -n "$out" ] || { echo "--out is required for backup" >&2; exit 2; }
    [ ! -e "$out" ] || { echo "refusing to overwrite existing backup: $out" >&2; exit 2; }
    echo "Reading the original flash to $out; put the module PCB in serial boot mode first."
    exec stm32flash -b 115200 -r "$out" "$port"
    ;;
  write)
    [ -n "$hex" ] || { echo "--hex is required for write" >&2; exit 2; }
    [ -f "$hex" ] || { echo "HEX file not found: $hex" >&2; exit 2; }
    [ "$confirm" = true ] && [ "$allow_write" = true ] || {
      echo "refusing to write: pass --confirm --allow-write explicitly" >&2
      exit 2
    }
    case "$hex" in
      *.hex|*.HEX) ;;
      *) echo "refusing non-Intel-HEX artifact: $hex" >&2; exit 2 ;;
    esac
    echo "Writing $hex to $port; the board must already be in serial boot mode."
    exec stm32flash -b 115200 -w "$hex" -v -g 0x08000000 "$port"
    ;;
  *) usage >&2; exit 2 ;;
esac
