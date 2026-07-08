#!/bin/bash
# Antigravity OS — Phase 3 Build Automation Script
set -e

# Get project root directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILD_DIR="$ROOT_DIR/build"

echo "Resetting build directory..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

cd "$BUILD_DIR"

echo "Configuring projects with CMake..."
cmake -S "$ROOT_DIR" -B "$BUILD_DIR"

echo "Building bootloader and kernel..."
cmake --build "$BUILD_DIR"

echo "=============================================="
echo " Build successful!"
echo " Bootloader: $BUILD_DIR/bootloader/BOOTX64.EFI.exe"
echo " Kernel:     $BUILD_DIR/kernel/kernel.elf"
echo "=============================================="
