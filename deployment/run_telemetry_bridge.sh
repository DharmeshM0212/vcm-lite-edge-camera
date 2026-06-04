#!/usr/bin/env bash
set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

SERIAL_PORT="${SERIAL_PORT:-}"
BAUDRATE="${BAUDRATE:-115200}"

cd "${PROJECT_ROOT}/telemetry_bridge"

if [ ! -d ".venv" ]; then
    python3 -m venv .venv
fi

. .venv/bin/activate

pip install -r requirements.txt

if [ -z "${SERIAL_PORT}" ]; then
    python telemetry_uart_sender.py \
      --metrics-log "${PROJECT_ROOT}/logs/metrics.jsonl" \
      --baudrate "${BAUDRATE}"
else
    python telemetry_uart_sender.py \
      --metrics-log "${PROJECT_ROOT}/logs/metrics.jsonl" \
      --serial-port "${SERIAL_PORT}" \
      --baudrate "${BAUDRATE}"
fi