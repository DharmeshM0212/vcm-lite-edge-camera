#!/usr/bin/env bash
set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "Stopping VCM-Lite Uno Q services"

pkill -f "uvicorn main:app --host 0.0.0.0 --port 9000" || true
pkill -f "python receiver.py" || true
pkill -f "vcm_lite_engine" || true
pkill -f "uvicorn main:app --host 0.0.0.0 --port 8000" || true

if [ -d "${PROJECT_ROOT}/logs/runtime" ]; then
    rm -f "${PROJECT_ROOT}/logs/runtime/"*.pid || true
fi

echo "Stopped"