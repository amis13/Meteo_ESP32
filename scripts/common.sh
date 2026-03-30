#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEFAULT_FQBN="esp32:esp32:esp32"
DEFAULT_BAUDRATE="115200"

detect_arduino_cli() {
  if [[ -n "${ARDUINO_CLI_BIN:-}" ]]; then
    printf '%s\n' "$ARDUINO_CLI_BIN"
    return
  fi

  if command -v arduino-cli >/dev/null 2>&1; then
    command -v arduino-cli
    return
  fi

  if [[ -x "/snap/arduino-cli/current/usr/bin/arduino-cli" ]]; then
    printf '%s\n' "/snap/arduino-cli/current/usr/bin/arduino-cli"
    return
  fi

  printf 'No encuentro arduino-cli en PATH.\n' >&2
  exit 1
}

ARDUINO_CLI_BIN="$(detect_arduino_cli)"
ARDUINO_FQBN="${ARDUINO_FQBN:-$DEFAULT_FQBN}"
MONITOR_BAUDRATE="${MONITOR_BAUDRATE:-$DEFAULT_BAUDRATE}"

if [[ -z "${ARDUINO_CONFIG_DIR:-}" && -d "${HOME}/snap/arduino-cli/current/.arduino15" ]]; then
  ARDUINO_CONFIG_DIR="${HOME}/snap/arduino-cli/current/.arduino15"
fi

if [[ -z "${ARDUINO_LIBRARIES:-}" && -d "${HOME}/snap/arduino-cli/current/Arduino/libraries" ]]; then
  ARDUINO_LIBRARIES="${HOME}/snap/arduino-cli/current/Arduino/libraries"
fi

arduino_cli() {
  local cmd=("$ARDUINO_CLI_BIN")

  if [[ -n "${ARDUINO_CONFIG_DIR:-}" ]]; then
    cmd+=(--config-dir "$ARDUINO_CONFIG_DIR")
  fi

  "${cmd[@]}" "$@"
}

list_ports() {
  arduino_cli board list
}

compile_sketch() {
  local sketch_dir="$1"
  local cmd=(compile --fqbn "$ARDUINO_FQBN")

  if [[ -n "${ARDUINO_LIBRARIES:-}" ]]; then
    cmd+=(--libraries "$ARDUINO_LIBRARIES")
  fi

  cmd+=("$PROJECT_ROOT/$sketch_dir")
  arduino_cli "${cmd[@]}"
}

upload_sketch() {
  local sketch_dir="$1"
  local port="$2"

  arduino_cli upload -p "$port" --fqbn "$ARDUINO_FQBN" "$PROJECT_ROOT/$sketch_dir"
}

monitor_port() {
  local port="$1"

  arduino_cli monitor -p "$port" -c "baudrate=$MONITOR_BAUDRATE,dtr=off,rts=off"
}

require_port_arg() {
  if [[ $# -lt 1 || -z "${1:-}" ]]; then
    printf 'Uso: %s <puerto>\n' "$0" >&2
    exit 1
  fi
}
