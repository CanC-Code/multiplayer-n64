#!/bin/bash

set -e

echo "[*] Cleaning previous build..."
rm -rf build
mkdir -p build

echo "[*] Collecting source files..."
SRC=$(find mupen64plus-next/mupen64plus-core/src -name "*.c")

echo "[*] Setting include paths..."
INCLUDES="-Imupen64plus-next \
-Imupen64plus-next/custom \
-Imupen64plus-next/libretro-common/include \
-Imupen64plus-next/mupen64plus-core/src/api \
-Imupen64plus-next/mupen64plus-core/src/main \
-Imupen64plus-next/mupen64plus-core/src/device/r4300 \
-Imupen64plus-next/mupen64plus-core/src/input \
-Imupen64plus-next/mupen64plus-core/src/plugin \
-Imupen64plus-next/mupen64plus-core/src"

echo "[*] Compiling with Emscripten..."
emcc $SRC $INCLUDES \
    -O3 \
    -s USE_SDL=2 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s ASSERTIONS=1 \
    -o build/emulator.js

echo "[*] Build complete: build/emulator.js"
