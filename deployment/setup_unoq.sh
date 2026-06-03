#!/usr/bin/env bash
set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "Project root: ${PROJECT_ROOT}"

sudo apt update

sudo apt install -y \
  build-essential \
  cmake \
  ninja-build \
  pkg-config \
  python3 \
  python3-venv \
  python3-pip \
  git \
  libopencv-dev

cd "${PROJECT_ROOT}"

mkdir -p logs outputs models

echo "Creating receiver virtual environment"
cd "${PROJECT_ROOT}/webrtc_receiver"
python3 -m venv .venv
. .venv/bin/activate
pip install --upgrade pip
pip install -r requirements.txt
deactivate

echo "Creating control plane virtual environment"
cd "${PROJECT_ROOT}/control_plane"
python3 -m venv .venv
. .venv/bin/activate
pip install --upgrade pip
pip install -r requirements.txt
deactivate

echo "Building C++ engine"
cd "${PROJECT_ROOT}/cpp_engine"
rm -rf build
mkdir -p build
cd build
cmake .. -G Ninja
cmake --build .

echo "Setup complete"
echo "Make sure models/object_detector.onnx and models/labels.txt exist before running the engine"