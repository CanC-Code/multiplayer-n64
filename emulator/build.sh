#!/bin/bash
set -e

echo "[1/3] Cleaning build..."
rm -rf build dist
mkdir -p build dist

echo "[2/3] Preparing (core not added yet)..."
# Later we will add mupen64plus-libretro-nx as a submodule here

echo "[3/3] Build complete (placeholder)."

