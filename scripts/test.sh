#!/bin/bash
# Antigravity OS — local test and regression runner
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
BOOTLOADER_EFI="$BUILD_DIR/bootloader/BOOTX64.EFI.exe"
KERNEL_ELF="$BUILD_DIR/kernel/kernel.elf"

PASSED=0

pass() {
    PASSED=$((PASSED + 1))
    echo "[PASS] $1"
}

require_file() {
    if [ ! -s "$1" ]; then
        echo "[FAIL] Missing or empty file: $1"
        exit 1
    fi
    pass "$2"
}

require_text() {
    local file="$1"
    local pattern="$2"
    local label="$3"

    if ! grep -q -- "$pattern" "$file"; then
        echo "[FAIL] $label"
        echo "       Pattern '$pattern' not found in $file"
        exit 1
    fi
    pass "$label"
}

echo "== Build tests =="
"$SCRIPT_DIR/build.sh"
require_file "$BOOTLOADER_EFI" "bootloader artifact exists"
require_file "$KERNEL_ELF" "kernel artifact exists"
require_text "$ROOT_DIR/kernel/CMakeLists.txt" "-ffreestanding" "freestanding compile flag enabled"
require_text "$ROOT_DIR/kernel/CMakeLists.txt" "x86_64-elf-ld" "freestanding linker path enabled"

echo
echo "== Boot tests =="
require_text "$ROOT_DIR/scripts/run.sh" "EFI/BOOT/BOOTX64.EFI" "run script packages UEFI boot path"
require_text "$ROOT_DIR/scripts/run.sh" "kernel/kernel.elf" "run script packages kernel handoff path"
require_text "$ROOT_DIR/kernel/src/kernel.cpp" "KernelPanic" "kernel panic path present"
require_text "$ROOT_DIR/kernel/src/kernel.cpp" "KernelGuiInit" "desktop startup path present"
require_text "$ROOT_DIR/kernel/src/userspace.cpp" "SysExit" "userspace exit path present"

echo
echo "== Kernel self-tests =="
require_text "$ROOT_DIR/kernel/src/kernel.cpp" "KernelMemorySelfTest" "memory manager self-test present"
require_text "$ROOT_DIR/kernel/src/scheduler.cpp" "KernelSchedulerRunSelfTest" "scheduler/process self-test present"
require_text "$ROOT_DIR/kernel/src/drivers.cpp" "RunPs2PacketDecoderSelfTest" "PS/2 packet decoder self-test present"
require_text "$ROOT_DIR/kernel/src/drivers.cpp" "RunUsbHidSelfTest" "USB HID descriptor parser self-test present"
require_text "$ROOT_DIR/kernel/src/cpu.cpp" "CpuIrqHandler" "CPU interrupt delivery handler present"
require_text "$ROOT_DIR/kernel/src/filesystem.cpp" "RunFat32ProbeSelfTest" "filesystem FAT32 self-test present"
require_text "$ROOT_DIR/kernel/src/userspace.cpp" "KernelSyscall" "userspace syscall self-test target present"

echo
echo "== Driver tests =="
require_text "$ROOT_DIR/kernel/src/drivers.cpp" "ScanPciBus" "PCI enumeration test target present"
require_text "$ROOT_DIR/kernel/src/drivers.cpp" "PciWriteConfig" "PCI config-space read/write helpers present"
require_text "$ROOT_DIR/kernel/src/drivers.cpp" "InitKeyboard" "keyboard input driver path present"
require_text "$ROOT_DIR/kernel/src/drivers.cpp" "InitMouse" "mouse input driver path present"
require_text "$ROOT_DIR/kernel/src/drivers.cpp" "ahci_controller_count" "storage controller discovery path present"
require_text "$ROOT_DIR/kernel/src/drivers.cpp" "virtio_net_supported" "network controller discovery path present"

echo
echo "== UI tests =="
require_text "$ROOT_DIR/kernel/src/gui.cpp" "WindowManager" "window manager interaction target present"
require_text "$ROOT_DIR/kernel/src/gui.cpp" "help mem cpu clear version uptime" "terminal command surface present"
require_text "$ROOT_DIR/kernel/src/gui.cpp" "MouseDown" "mouse click path present"
require_text "$ROOT_DIR/kernel/src/gui.cpp" "Drag" "mouse drag path present"
require_text "$ROOT_DIR/kernel/src/gui.cpp" "ComposeDesktop" "desktop redraw path present"
require_text "$ROOT_DIR/kernel/src/gui.cpp" "DrawRect" "graphics primitive rendering path present"

echo
echo "== Regression tests =="
require_text "$ROOT_DIR/scripts/test.sh" "scripts/run.sh" "automated local test runner present"
require_text "$ROOT_DIR/kernel/src/kernel.cpp" "KernelLog" "boot log check target present"
require_text "$ROOT_DIR/kernel/src/kernel.cpp" "FreePages" "memory regression check target present"
require_text "$ROOT_DIR/kernel/src/scheduler.cpp" "priority" "scheduler fairness regression target present"
require_text "$ROOT_DIR/kernel/src/filesystem.cpp" "KernelFileSystemInit" "filesystem regression target present"

echo
echo "=============================================="
echo " Test runner completed: $PASSED checks passed"
echo "=============================================="
