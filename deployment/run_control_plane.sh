#!/usr/bin/env bash
set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "${PROJECT_ROOT}/control_plane"

. .venv/bin/activate

uvicorn main:app --host 0.0.0.0 --port 8000