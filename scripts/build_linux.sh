#!/usr/bin/env bash
set -euo pipefail

CONFIG="${1:-Debug}"

if [ "${CONFIG}" = "Release" ]; then
    PRESET="linux-x64-release"
else
    PRESET="linux-x64-debug"
    CONFIG="Debug"
fi

echo "=== Configuring ${PRESET} ==="
cmake --preset "${PRESET}"

echo "=== Building ${PRESET} ==="
cmake --build "build/${PRESET}"

echo "=== Testing ${PRESET} ==="
ctest --test-dir "build/${PRESET}" --output-on-failure
