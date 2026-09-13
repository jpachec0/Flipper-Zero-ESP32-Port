#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
EXPORT_SCRIPT="${ESP_IDF_EXPORT_SCRIPT:-${HOME}/esp/esp-idf/export.sh}"
OUT_DIR="${1:-internal_ext_flash_package}"

cd "$ROOT"

BRANCH="$(git branch --show-current 2>/dev/null || true)"
if [[ "$BRANCH" != "feature/internal-ext-fallback" ]]; then
  echo "ERROR: checkout feature/internal-ext-fallback first (current: ${BRANCH:-unknown})." >&2
  exit 1
fi

if [[ ! -f "$EXPORT_SCRIPT" ]]; then
  echo "ERROR: ESP-IDF export script not found: $EXPORT_SCRIPT" >&2
  echo "Install/source ESP-IDF v5.4.1 or set ESP_IDF_EXPORT_SCRIPT." >&2
  exit 1
fi

source "$EXPORT_SCRIPT"
IDF_VER="$(idf.py --version 2>/dev/null || true)"
if [[ "$IDF_VER" != *"5.4.1"* ]]; then
  echo "ERROR: this port requires ESP-IDF v5.4.1; detected: ${IDF_VER:-unknown}" >&2
  exit 1
fi

EXPECTED='ext_fat,    data, fat,       0x490000,  0xB50000,'
if ! grep -Fq "$EXPECTED" partitions_ota_16mb.csv; then
  echo "ERROR: unexpected T-Embed internal-ext partition layout." >&2
  exit 1
fi

echo "=== Building LilyGo T-Embed CC1101 internal-/ext firmware ==="
rm -f sdkconfig build_t_embed/sdkconfig
bash ./build.sh --board t_embed --build-only

APP="build_t_embed/furi_esp32.bin"
[[ -f "$APP" ]] || { echo "ERROR: firmware binary missing after build" >&2; exit 1; }
APP_SIZE=$(stat -c%s "$APP" 2>/dev/null || stat -f%z "$APP")
APP_LIMIT=$((0x480000))
if (( APP_SIZE >= APP_LIMIT )); then
  echo "ERROR: firmware is ${APP_SIZE} bytes and does not fit 4.5 MiB app partition." >&2
  exit 1
fi

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR/bootloader" "$OUT_DIR/partition_table"
cp build_t_embed/furi_esp32.bin "$OUT_DIR/furi_esp32.bin"
cp build_t_embed/bootloader/bootloader.bin "$OUT_DIR/bootloader/bootloader.bin"
cp build_t_embed/partition_table/partition-table.bin "$OUT_DIR/partition_table/partition-table.bin"
if [[ -f build_t_embed/flash_project_args ]]; then
  cp build_t_embed/flash_project_args "$OUT_DIR/flash_args.txt"
fi
sed \
  -e 's/__BOARD_NAME__/lilygo_t_embed_cc1101/g' \
  -e 's/__CHIP__/esp32s3/g' \
  -e 's/__FLASH_SIZE__/16MB/g' \
  tools/flash_linux.sh > "$OUT_DIR/flash_linux.sh"
chmod +x "$OUT_DIR/flash_linux.sh"
sed \
  -e 's/__BOARD_NAME__/lilygo_t_embed_cc1101/g' \
  -e 's/__CHIP__/esp32s3/g' \
  -e 's/__FLASH_SIZE__/16MB/g' \
  tools/flash_windows.bat > "$OUT_DIR/flash_windows.bat"

printf 'Firmware size: %d bytes\n' "$APP_SIZE"
echo "Package ready: $OUT_DIR"
echo "For the first migration to this partition layout, perform a full flash/erase explicitly before flashing this package."
