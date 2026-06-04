#!/usr/bin/env bash
set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_DIR="${PROJECT_ROOT}/logs/runtime"

check_pid_file() {
    local name="$1"
    local pid_file="$2"

    if [ ! -f "${pid_file}" ]; then
        echo "${name}: not started"
        return
    fi

    local pid
    pid="$(cat "${pid_file}")"

    if kill -0 "${pid}" 2>/dev/null; then
        echo "${name}: running pid=${pid}"
    else
        echo "${name}: dead pid=${pid}"
    fi
}

echo "VCM-Lite Uno Q service status"
echo "Project: ${PROJECT_ROOT}"
echo ""

check_pid_file "signaling_server" "${LOG_DIR}/signaling_server.pid"
check_pid_file "webrtc_receiver" "${LOG_DIR}/webrtc_receiver.pid"
check_pid_file "cpp_engine" "${LOG_DIR}/cpp_engine.pid"
check_pid_file "control_plane" "${LOG_DIR}/control_plane.pid"
check_pid_file "internal_mcu_telemetry" "${LOG_DIR}/internal_mcu_telemetry.pid"

echo ""
echo "Processes:"
pgrep -af "uvicorn main:app|python receiver.py|vcm_lite_engine|internal_mcu_monitor_sender.py" || true

echo ""
echo "Ports:"
ss -ltnp 2>/dev/null | grep -E "(:9000|:8000|:5001|:7500)" || true

echo ""
echo "Control plane health:"
curl -s http://127.0.0.1:8000/health || echo "control plane not reachable"

echo ""
echo ""
echo "Signaling health:"
curl -s http://127.0.0.1:9000/health || echo "signaling server not reachable"

echo ""
echo ""
echo "Latest metrics:"
if [ -f "${PROJECT_ROOT}/logs/metrics.jsonl" ]; then
    tail -n 1 "${PROJECT_ROOT}/logs/metrics.jsonl"
else
    echo "no metrics log yet"
fi

echo ""
echo "Latest WebRTC:"
if [ -f "${PROJECT_ROOT}/logs/webrtc_receiver.jsonl" ]; then
    tail -n 1 "${PROJECT_ROOT}/logs/webrtc_receiver.jsonl"
else
    echo "no WebRTC log yet"
fi