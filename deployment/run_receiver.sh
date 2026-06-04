#!/usr/bin/env bash
set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

SIGNALING_URL="${SIGNALING_URL:-http://127.0.0.1:9000}"
FRAME_HOST="${FRAME_HOST:-127.0.0.1}"
FRAME_PORT="${FRAME_PORT:-5001}"

cd "${PROJECT_ROOT}/webrtc_receiver"

. .venv/bin/activate

python receiver.py \
  --signaling-url "${SIGNALING_URL}" \
  --output-dir "${PROJECT_ROOT}/outputs" \
  --log-path "${PROJECT_ROOT}/logs/webrtc_receiver.jsonl" \
  --frame-host "${FRAME_HOST}" \
  --frame-port "${FRAME_PORT}" \
  --save-debug-frames