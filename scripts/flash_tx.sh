#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

require_port_arg "$@"
PORT="$1"

compile_sketch "esp32_bme_tx"
upload_sketch "esp32_bme_tx" "$PORT"

printf 'Emisor grabado en %s\n' "$PORT"
