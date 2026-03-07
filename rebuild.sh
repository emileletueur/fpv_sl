#!/usr/bin/env bash
# rebuild.sh — clean, configure et build fpv_sl
# Usage:
#   ./rebuild.sh          # production (WS2812)
#   ./rebuild.sh debug    # debug probe (onboard LED GP25, GPIO remappés)

set -e

BUILD_DIR="src/build"
CMAKE_OPTS=""

if [ "$1" = "debug" ]; then
    CMAKE_OPTS="-DFPV_SL_PICO_PROBE_DEBUG=ON"
    echo "[rebuild] Mode: DEBUG (probe)"
else
    echo "[rebuild] Mode: PRODUCTION"
fi

echo "[rebuild] Suppression de $BUILD_DIR..."
rm -rf "$BUILD_DIR"

echo "[rebuild] Configuration CMake..."
cmake -S src -B "$BUILD_DIR" -G Ninja $CMAKE_OPTS

echo "[rebuild] Build..."
cmake --build "$BUILD_DIR"

echo "[rebuild] OK — binaire : $BUILD_DIR/fpv_sl_loader.uf2"
