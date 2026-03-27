#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

require_port_arg "$@"
PORT="$1"

compile_sketch "esp32_oled_rx"
upload_sketch "esp32_oled_rx" "$PORT"

printf 'Receptor grabado en %s\n' "$PORT"
