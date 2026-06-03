#!/usr/bin/env bash
set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

FRAME_HOST="${FRAME_HOST:-127.0.0.1}"
FRAME_PORT="${FRAME_PORT:-5001}"

cd "${PROJECT_ROOT}/cpp_engine/build"

./vcm_lite_engine \
  "tcp://${FRAME_HOST}:${FRAME_PORT}" \
  "${PROJECT_ROOT}/logs/metrics.jsonl" \
  "${PROJECT_ROOT}/logs/metadata.jsonl" \
  "${PROJECT_ROOT}/models/object_detector.onnx" \
  "${PROJECT_ROOT}/models/labels.txt" \
  "${PROJECT_ROOT}/outputs"