#!/usr/bin/env bash
set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_DIR="${PROJECT_ROOT}/logs/runtime"

mkdir -p "${LOG_DIR}"

echo "Tailing VCM-Lite runtime logs"
echo "Ctrl+C to stop viewing logs"
echo ""

touch "${LOG_DIR}/signaling_server.log"
touch "${LOG_DIR}/webrtc_receiver.log"
touch "${LOG_DIR}/cpp_engine.log"
touch "${LOG_DIR}/control_plane.log"
touch "${LOG_DIR}/internal_mcu_telemetry.log"

tail -n 40 -f \
    "${LOG_DIR}/signaling_server.log" \
    "${LOG_DIR}/webrtc_receiver.log" \
    "${LOG_DIR}/cpp_engine.log" \
    "${LOG_DIR}/control_plane.log" \
    "${LOG_DIR}/internal_mcu_telemetry.log"