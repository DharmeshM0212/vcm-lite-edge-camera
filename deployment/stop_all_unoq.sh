#!/usr/bin/env bash
set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_DIR="${PROJECT_ROOT}/logs/runtime"

echo "Stopping VCM-Lite Uno Q services"

if [ -d "${LOG_DIR}" ]; then
    for pid_file in \
        "${LOG_DIR}/internal_mcu_telemetry.pid" \
        "${LOG_DIR}/control_plane.pid" \
        "${LOG_DIR}/cpp_engine.pid" \
        "${LOG_DIR}/webrtc_receiver.pid" \
        "${LOG_DIR}/signaling_server.pid"
    do
        if [ -f "${pid_file}" ]; then
            pid="$(cat "${pid_file}")"

            if kill -0 "${pid}" 2>/dev/null; then
                echo "stopping pid ${pid} from ${pid_file}"
                kill "${pid}" || true
            fi

            rm -f "${pid_file}"
        fi
    done
fi

sleep 1

pkill -f "internal_mcu_monitor_sender.py" || true
pkill -f "uvicorn main:app --host 0.0.0.0 --port 8000" || true
pkill -f "vcm_lite_engine" || true
pkill -f "python receiver.py" || true
pkill -f "uvicorn main:app --host 0.0.0.0 --port 9000" || true

sleep 1

echo "Stopped"