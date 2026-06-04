#!/usr/bin/env bash
set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

MONITOR_HOST="${MONITOR_HOST:-127.0.0.1}"
MONITOR_PORT="${MONITOR_PORT:-7500}"

cd "${PROJECT_ROOT}/telemetry_bridge"

python3 internal_mcu_monitor_sender.py \
  --metrics-log "${PROJECT_ROOT}/logs/metrics.jsonl" \
  --host "${MONITOR_HOST}" \
  --port "${MONITOR_PORT}"