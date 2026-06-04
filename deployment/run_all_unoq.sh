#!/usr/bin/env bash
set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_DIR="${PROJECT_ROOT}/logs/runtime"

mkdir -p "${LOG_DIR}"

echo "Starting VCM-Lite services"
echo "Logs: ${LOG_DIR}"

pkill -f "uvicorn main:app --host 0.0.0.0 --port 9000" || true
pkill -f "python receiver.py" || true
pkill -f "vcm_lite_engine" || true
pkill -f "uvicorn main:app --host 0.0.0.0 --port 8000" || true

sleep 1

nohup "${PROJECT_ROOT}/deployment/run_signaling_server.sh" > "${LOG_DIR}/signaling_server.log" 2>&1 &
echo $! > "${LOG_DIR}/signaling_server.pid"

sleep 2

nohup "${PROJECT_ROOT}/deployment/run_receiver.sh" > "${LOG_DIR}/receiver.log" 2>&1 &
echo $! > "${LOG_DIR}/receiver.pid"

sleep 2

nohup "${PROJECT_ROOT}/deployment/run_engine.sh" > "${LOG_DIR}/engine.log" 2>&1 &
echo $! > "${LOG_DIR}/engine.pid"

sleep 2

nohup "${PROJECT_ROOT}/deployment/run_control_plane.sh" > "${LOG_DIR}/control_plane.log" 2>&1 &
echo $! > "${LOG_DIR}/control_plane.pid"

echo "Started:"
echo "  signaling server: http://0.0.0.0:9000"
echo "  control plane:    http://0.0.0.0:8000"
echo "  frame socket:     tcp://127.0.0.1:5001"
echo ""
echo "From laptop sender, use:"
echo "  python dataset_sender.py --video \"../assets/videos/input.mp4\" --signaling-url \"http://<UNOQ_IP>:9000\" --loop --reset"