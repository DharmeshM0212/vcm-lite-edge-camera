#!/usr/bin/env bash
set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_DIR="${PROJECT_ROOT}/logs/runtime"

mkdir -p "${LOG_DIR}"
mkdir -p "${PROJECT_ROOT}/logs"
mkdir -p "${PROJECT_ROOT}/outputs"

wait_for_http() {
    local name="$1"
    local url="$2"
    local max_attempts="${3:-30}"

    echo "waiting for ${name}: ${url}"

    for i in $(seq 1 "${max_attempts}"); do
        if curl -s "${url}" >/dev/null 2>&1; then
            echo "${name}: ready"
            return 0
        fi

        sleep 1
    done

    echo "${name}: not ready after ${max_attempts}s"
    return 1
}

wait_for_port() {
    local name="$1"
    local host="$2"
    local port="$3"
    local max_attempts="${4:-30}"

    echo "waiting for ${name}: ${host}:${port}"

    for i in $(seq 1 "${max_attempts}"); do
        if timeout 1 bash -c "cat < /dev/null > /dev/tcp/${host}/${port}" >/dev/null 2>&1; then
            echo "${name}: ready"
            return 0
        fi

        sleep 1
    done

    echo "${name}: not ready after ${max_attempts}s"
    return 1
}

start_service() {
    local name="$1"
    local command="$2"
    local log_file="${LOG_DIR}/${name}.log"
    local pid_file="${LOG_DIR}/${name}.pid"

    echo ""
    echo "starting ${name}"

    nohup bash -lc "${command}" > "${log_file}" 2>&1 &
    local pid="$!"

    echo "${pid}" > "${pid_file}"

    sleep 1

    if kill -0 "${pid}" 2>/dev/null; then
        echo "${name}: launched pid=${pid}"
    else
        echo "${name}: failed immediately"
        echo "---- ${log_file} ----"
        tail -n 100 "${log_file}" || true
        exit 1
    fi
}

echo "Stopping existing VCM-Lite services..."
"${PROJECT_ROOT}/deployment/stop_all_unoq.sh" || true

sleep 2

echo ""
echo "Starting VCM-Lite Uno Q services"
echo "Project: ${PROJECT_ROOT}"
echo "Runtime logs: ${LOG_DIR}"

start_service "signaling_server" "
cd '${PROJECT_ROOT}/signaling_server'
. .venv/bin/activate
exec uvicorn main:app --host 0.0.0.0 --port 9000
"

wait_for_http "signaling_server" "http://127.0.0.1:9000/health" 30

start_service "webrtc_receiver" "
cd '${PROJECT_ROOT}/webrtc_receiver'
. .venv/bin/activate
exec python receiver.py \
  --signaling-url 'http://127.0.0.1:9000' \
  --output-dir '${PROJECT_ROOT}/outputs' \
  --log-path '${PROJECT_ROOT}/logs/webrtc_receiver.jsonl' \
  --frame-host 127.0.0.1 \
  --frame-port 5001 \
  --save-debug-frames
"

wait_for_port "webrtc_receiver_frame_socket" "127.0.0.1" "5001" 30

start_service "cpp_engine" "
cd '${PROJECT_ROOT}/cpp_engine/build'
exec ./vcm_lite_engine \
  'tcp://127.0.0.1:5001' \
  '${PROJECT_ROOT}/logs/metrics.jsonl' \
  '${PROJECT_ROOT}/logs/metadata.jsonl' \
  '${PROJECT_ROOT}/models/object_detector.onnx' \
  '${PROJECT_ROOT}/models/labels.txt' \
  '${PROJECT_ROOT}/outputs'
"

sleep 2

if ! pgrep -af "vcm_lite_engine" >/dev/null 2>&1; then
    echo "cpp_engine failed after launch"
    echo "---- ${LOG_DIR}/cpp_engine.log ----"
    tail -n 120 "${LOG_DIR}/cpp_engine.log" || true
    exit 1
fi

start_service "control_plane" "
cd '${PROJECT_ROOT}/control_plane'
. .venv/bin/activate
exec uvicorn main:app --host 0.0.0.0 --port 8000
"

wait_for_http "control_plane" "http://127.0.0.1:8000/health" 30

if [ -f "${PROJECT_ROOT}/telemetry_bridge/internal_mcu_monitor_sender.py" ]; then
    start_service "internal_mcu_telemetry" "
cd '${PROJECT_ROOT}/telemetry_bridge'
exec python3 internal_mcu_monitor_sender.py \
  --metrics-log '${PROJECT_ROOT}/logs/metrics.jsonl' \
  --host 127.0.0.1 \
  --port 7500
"
else
    echo ""
    echo "internal_mcu_telemetry: skipped, script not found"
fi

echo ""
echo "All board services launched."
echo ""
"${PROJECT_ROOT}/deployment/status_unoq.sh" || true

echo ""
echo "Now run laptop sender:"
echo "cd \"C:\\Users\\DHARMESH M\\Documents\\Projects\\vcm-lite-edge-camera\\sensor_sender\""
echo ".\\.venv\\Scripts\\activate"
echo "python dataset_sender.py --video-dir \"../assets/videos/demo\" --signaling-url \"http://10.127.210.82:9000\" --resize-width 640 --max-fps 15 --reset"
echo ""
echo "Dashboard:"
echo "http://10.127.210.82:8000/dashboard?t=1"
echo ""
echo "Logs:"
echo "./deployment/tail_unoq_logs.sh"