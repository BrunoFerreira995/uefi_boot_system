#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "[Step 1] Preparing project environment..."

chmod +x "$SCRIPT_DIR/build.sh" "$SCRIPT_DIR/run.sh"

if command -v cmake >/dev/null 2>&1; then
    echo "[OK] CMake found"
else
    echo "[ERROR] CMake not found. Please install CMake first."
    exit 1
fi

if command -v clang >/dev/null 2>&1 && command -v clang++ >/dev/null 2>&1; then
    echo "[OK] Clang found"
else
    echo "[ERROR] Clang/LLVM not found. Please install Clang/LLVM first."
    exit 1
fi

if command -v qemu-system-x86_64 >/dev/null 2>&1; then
    echo "[OK] QEMU found"
else
    echo "[WARNING] QEMU not found. Install it if you want to run the project."
fi

if [ -x /opt/homebrew/bin/x86_64-w64-mingw32-gcc ] || [ -x /opt/homebrew/bin/x86_64-w64-mingw32-g++ ]; then
    echo "[OK] MinGW cross-compiler found"
else
    echo "[WARNING] MinGW cross-compiler not found. Build may fail for the bootloader."
fi

if [ -x /opt/homebrew/bin/x86_64-elf-ld ]; then
    echo "[OK] x86_64-elf-ld found"
else
    echo "[WARNING] x86_64-elf-ld not found. Kernel linking may fail."
fi

echo "[Done] Step 1 environment check completed."
