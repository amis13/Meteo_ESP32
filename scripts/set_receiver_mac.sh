#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

if [[ $# -lt 1 || -z "${1:-}" ]]; then
  printf 'Uso: %s <MAC_receptor>\n' "$0" >&2
  printf 'Ejemplo: %s 24:6F:28:12:34:56\n' "$0" >&2
  exit 1
fi

INPUT_MAC="${1^^}"
if [[ ! "$INPUT_MAC" =~ ^([0-9A-F]{2}:){5}[0-9A-F]{2}$ ]]; then
  printf 'MAC invalida: %s\n' "$1" >&2
  exit 1
fi

IFS=':' read -r B1 B2 B3 B4 B5 B6 <<<"$INPUT_MAC"
printf -v MAC_ARRAY '0x%s, 0x%s, 0x%s, 0x%s, 0x%s, 0x%s' \
  "$B1" "$B2" "$B3" "$B4" "$B5" "$B6"

TARGET_FILE="$PROJECT_ROOT/esp32_bme_tx/esp32_bme_tx.ino"
if ! grep -q '^uint8_t RECEIVER_MAC\[6\] = {' "$TARGET_FILE"; then
  printf 'No encuentro RECEIVER_MAC en %s\n' "$TARGET_FILE" >&2
  exit 1
fi

sed -i -E "s/^uint8_t RECEIVER_MAC\[6\] = \{.*\};$/uint8_t RECEIVER_MAC[6] = {$MAC_ARRAY};/" "$TARGET_FILE"

printf 'MAC del receptor actualizada a %s\n' "$INPUT_MAC"
printf 'Linea nueva:\n'
grep '^uint8_t RECEIVER_MAC\[6\] = {' "$TARGET_FILE"
