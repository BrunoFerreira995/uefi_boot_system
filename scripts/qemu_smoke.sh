#!/bin/bash
# Antigravity OS runtime verification smoke test.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
ESP_DIR="$ROOT_DIR/ESP"
ARTIFACT_DIR="${QEMU_SMOKE_ARTIFACT_DIR:-$ROOT_DIR/build/qemu-smoke}"
SERIAL_LOG="$ARTIFACT_DIR/serial.log"
SCREENSHOT="$ARTIFACT_DIR/desktop.ppm"
MONITOR_IN="$ARTIFACT_DIR/monitor.in"
BOOTLOADER_EFI="$BUILD_DIR/bootloader/BOOTX64.EFI.exe"
KERNEL_ELF="$BUILD_DIR/kernel/kernel.elf"
TIMEOUT_SECONDS="${QEMU_SMOKE_TIMEOUT:-60}"

find_ovmf() {
    local paths=(
        "/opt/homebrew/share/qemu/edk2-x86_64-code.fd"
        "/opt/homebrew/Cellar/qemu/"*"/share/qemu/edk2-x86_64-code.fd"
        "/usr/share/OVMF/OVMF_CODE.fd"
        "/usr/share/ovmf/x64/OVMF_CODE.fd"
        "/usr/share/edk2/ovmf/OVMF_CODE.fd"
        "/usr/share/ovmf/OVMF.fd"
    )

    local path
    for path in "${paths[@]}"; do
        if [ -f "$path" ]; then
            echo "$path"
            return 0
        fi
    done
    return 1
}

prepare_esp() {
    if [ ! -s "$BOOTLOADER_EFI" ] || [ ! -s "$KERNEL_ELF" ]; then
        "$SCRIPT_DIR/build.sh"
    fi

    rm -rf "$ESP_DIR"
    mkdir -p "$ESP_DIR/EFI/BOOT" "$ESP_DIR/kernel"
    cp "$BOOTLOADER_EFI" "$ESP_DIR/EFI/BOOT/BOOTX64.EFI"
    cp "$KERNEL_ELF" "$ESP_DIR/kernel/kernel.elf"
}

require_text() {
    local pattern="$1"
    local label="$2"
    if ! grep -q -- "$pattern" "$SERIAL_LOG"; then
        echo "[FAIL] $label"
        echo "       Pattern '$pattern' not found in $SERIAL_LOG"
        exit 1
    fi
    echo "[PASS] $label"
}

require_no_text() {
    local pattern="$1"
    local label="$2"
    if grep -q -- "$pattern" "$SERIAL_LOG"; then
        echo "[FAIL] $label"
        echo "       Unexpected '$pattern' found in $SERIAL_LOG"
        exit 1
    fi
    echo "[PASS] $label"
}

verify_screenshot() {
    if [ ! -s "$SCREENSHOT" ]; then
        echo "[FAIL] QEMU screendump was not created"
        exit 1
    fi
    if ! head -n 1 "$SCREENSHOT" | grep -q "P6"; then
        echo "[FAIL] QEMU screendump is not a PPM image"
        exit 1
    fi
    echo "[PASS] QEMU screendump captured video evidence"
}

run_qemu() {
    local ovmf="$1"
    mkdir -p "$ARTIFACT_DIR"
    : > "$SERIAL_LOG"
    {
        sleep 8
        echo "screendump $SCREENSHOT"
        sleep "$TIMEOUT_SECONDS"
        echo "quit"
    } > "$MONITOR_IN" &
    local monitor_pid=$!

    qemu-system-x86_64 \
        -drive if=pflash,format=raw,readonly=on,file="$ovmf" \
        -drive file=fat:rw:"$ESP_DIR",format=raw \
        -serial "file:$SERIAL_LOG" \
        -monitor stdio \
        -net none \
        -m 256M \
        -vga std < "$MONITOR_IN" || true

    wait "$monitor_pid" 2>/dev/null || true
}

if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
    echo "[SKIP] qemu-system-x86_64 not found"
    exit 0
fi

OVMF="$(find_ovmf || true)"
if [ -z "$OVMF" ]; then
    echo "[SKIP] OVMF firmware not found"
    exit 0
fi

prepare_esp
run_qemu "$OVMF"

require_text "Phase 10 GUI initialized" "boot reaches initialized desktop"
require_text "Compositor rendered desktop" "serial log contains compositor evidence"
require_text "Desktop environment ready" "serial log contains desktop-ready evidence"
require_no_text "\\[PANIC\\]" "60-second interaction run records no panic"
verify_screenshot

echo "Artifacts:"
echo "  serial:     $SERIAL_LOG"
echo "  screenshot: $SCREENSHOT"
