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
require_text "$ROOT_DIR/kernel/src/kernel.cpp" "HugePageManager" "huge-page manager present"
require_text "$ROOT_DIR/kernel/src/kernel.cpp" "NumaManager" "NUMA allocation manager present"
require_text "$ROOT_DIR/kernel/src/kernel.cpp" "MemoryMappingManager" "mmap and demand-paging manager present"
require_text "$ROOT_DIR/kernel/src/kernel.cpp" "SwapManager" "swap manager present"
require_text "$ROOT_DIR/kernel/src/kernel.cpp" "PageCache" "page cache present"
require_text "$ROOT_DIR/kernel/src/scheduler.cpp" "KernelSchedulerRunSelfTest" "scheduler/process self-test present"
require_text "$ROOT_DIR/kernel/src/scheduler.cpp" "KernelForkProcess" "fork process primitive present"
require_text "$ROOT_DIR/kernel/src/scheduler.cpp" "KernelExecProcess" "exec process primitive present"
require_text "$ROOT_DIR/kernel/src/scheduler.cpp" "KernelCloneThread" "clone thread primitive present"
require_text "$ROOT_DIR/kernel/src/scheduler.cpp" "KernelPthreadJoin" "pthread lifecycle support present"
require_text "$ROOT_DIR/kernel/src/scheduler.cpp" "KernelFutexWait" "futex wait/wake support present"
require_text "$ROOT_DIR/kernel/src/scheduler.cpp" "KernelEpollWait" "epoll readiness support present"
require_text "$ROOT_DIR/kernel/src/scheduler.cpp" "KernelEventFdRead" "eventfd counter support present"
require_text "$ROOT_DIR/kernel/src/scheduler.cpp" "KernelTimerFdRead" "timerfd expiration support present"
require_text "$ROOT_DIR/kernel/src/drivers.cpp" "RunPs2PacketDecoderSelfTest" "PS/2 packet decoder self-test present"
require_text "$ROOT_DIR/kernel/src/drivers.cpp" "RunUsbHidSelfTest" "USB HID descriptor parser self-test present"
require_text "$ROOT_DIR/kernel/src/drivers.cpp" "RunGraphicsDriverSelfTest" "DRM/GPU/buffering self-test present"
require_text "$ROOT_DIR/kernel/src/drivers.cpp" "GpuAllocate" "GPU memory manager present"
require_text "$ROOT_DIR/kernel/src/drivers.cpp" "RunAudioSelfTest" "PCM/mixer/Audio32 self-test present"
require_text "$ROOT_DIR/kernel/src/drivers.cpp" "uhci_supported" "USB host-controller classification present"
require_text "$ROOT_DIR/kernel/src/drivers.cpp" "RunUsbTransferSelfTest" "USB mass-storage transfer self-test present"
require_text "$ROOT_DIR/kernel/src/drivers.cpp" "RunExtendedInputSelfTest" "extended input self-test present"
require_text "$ROOT_DIR/kernel/src/drivers.cpp" "RunInputPipelineSelfTest" "PS/2 packet-to-GUI pipeline self-test present"
require_text "$ROOT_DIR/kernel/src/drivers.cpp" "IntelliMouse negotiation" "IntelliMouse wheel negotiation present"
require_text "$ROOT_DIR/kernel/src/drivers.cpp" "DecodeHidGamepadReport" "HID gamepad/joystick decoder present"
require_text "$ROOT_DIR/kernel/include/gui.hpp" "MouseWheel" "GUI mouse-wheel event present"
require_text "$ROOT_DIR/kernel/include/gui.hpp" "Gamepad" "GUI gamepad event present"
require_text "$ROOT_DIR/kernel/src/cpu.cpp" "CpuIrqHandler" "CPU interrupt delivery handler present"
require_text "$ROOT_DIR/kernel/src/cpu.cpp" "x2apic_supported" "x2APIC detection and enablement present"
require_text "$ROOT_DIR/kernel/src/cpu.cpp" "InitHpet" "ACPI HPET initialization present"
require_text "$ROOT_DIR/kernel/src/cpu.cpp" "InitLapicTimer" "local APIC scheduler timer present"
require_text "$ROOT_DIR/kernel/src/cpu.cpp" "DetectCpuFrequency" "CPU frequency detection present"
require_text "$ROOT_DIR/kernel/src/context_switch.S" "xsave64" "SIMD extended-state context save present"
require_text "$ROOT_DIR/kernel/src/context_switch.S" "xrstor64" "SIMD extended-state context restore present"
require_text "$ROOT_DIR/kernel/src/filesystem.cpp" "RunFat32ProbeSelfTest" "filesystem FAT32 self-test present"
require_text "$ROOT_DIR/kernel/src/filesystem.cpp" "RunPermissionSelfTest" "filesystem permission self-test present"
require_text "$ROOT_DIR/kernel/src/filesystem.cpp" "RunSymlinkSelfTest" "filesystem symlink self-test present"
require_text "$ROOT_DIR/kernel/src/filesystem.cpp" "RunMountManagerSelfTest" "filesystem mount manager self-test present"
require_text "$ROOT_DIR/kernel/src/filesystem.cpp" "RunRamFsSelfTest" "filesystem RamFS self-test present"
require_text "$ROOT_DIR/kernel/src/userspace.cpp" "KernelSyscall" "userspace syscall self-test target present"
require_text "$ROOT_DIR/kernel/src/userspace.cpp" "RunElfLoaderSelfTest" "userspace ELF loader self-test present"
require_text "$ROOT_DIR/kernel/src/userspace.cpp" "RunDynamicLinkerSelfTest" "userspace dynamic linker self-test present"
require_text "$ROOT_DIR/kernel/src/userspace.cpp" "RunLibcSelfTest" "userspace libc self-test present"
require_text "$ROOT_DIR/kernel/src/userspace.cpp" "RunPosixSelfTest" "userspace POSIX self-test present"
require_text "$ROOT_DIR/kernel/src/network.cpp" "RunEthernetSelfTest" "network Ethernet self-test present"
require_text "$ROOT_DIR/kernel/src/network.cpp" "RunArpSelfTest" "network ARP self-test present"
require_text "$ROOT_DIR/kernel/src/network.cpp" "RunIpv4SelfTest" "network IPv4 self-test present"
require_text "$ROOT_DIR/kernel/src/network.cpp" "RunIcmpSelfTest" "network ICMP self-test present"
require_text "$ROOT_DIR/kernel/src/network.cpp" "RunUdpSelfTest" "network UDP self-test present"
require_text "$ROOT_DIR/kernel/src/network.cpp" "RunTcpSelfTest" "network TCP self-test present"
require_text "$ROOT_DIR/kernel/src/network.cpp" "RunDhcpSelfTest" "network DHCP self-test present"
require_text "$ROOT_DIR/kernel/src/network.cpp" "RunDnsSelfTest" "network DNS self-test present"
require_text "$ROOT_DIR/kernel/src/network.cpp" "RunHttpSelfTest" "network HTTP self-test present"
require_text "$ROOT_DIR/kernel/src/network.cpp" "RunHttpsSelfTest" "network HTTPS self-test present"
require_text "$ROOT_DIR/kernel/src/network.cpp" "RunSocketApiSelfTest" "network socket API self-test present"
require_text "$ROOT_DIR/kernel/src/security.cpp" "RunUsersSelfTest" "security users self-test present"
require_text "$ROOT_DIR/kernel/src/security.cpp" "RunPermissionsSelfTest" "security permissions self-test present"
require_text "$ROOT_DIR/kernel/src/security.cpp" "RunAccessControlSelfTest" "security access control self-test present"
require_text "$ROOT_DIR/kernel/src/security.cpp" "RunProcessIsolationSelfTest" "security process isolation self-test present"
require_text "$ROOT_DIR/kernel/src/security.cpp" "RunVirtualMemoryProtectionSelfTest" "security virtual memory protection self-test present"

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
require_text "$ROOT_DIR/kernel/src/gui.cpp" "RunTerminalCommandSelfTest" "terminal command parser self-test present"
require_text "$ROOT_DIR/kernel/src/gui.cpp" "RunDesktopPhaseSelfTest" "desktop phase self-test present"
require_text "$ROOT_DIR/kernel/src/gui.cpp" "ResizeWindow" "window resize support present"
require_text "$ROOT_DIR/kernel/src/gui.cpp" "SnapWindow" "window snapping support present"
require_text "$ROOT_DIR/kernel/src/gui.cpp" "SwitchVirtualDesktop" "virtual desktop support present"
require_text "$ROOT_DIR/kernel/src/gui.cpp" "DecodePngHeader" "PNG header decoder present"
require_text "$ROOT_DIR/kernel/src/gui.cpp" "DecodeBmpHeader" "BMP header decoder present"
require_text "$ROOT_DIR/kernel/src/gui.cpp" "DecodeJpegHeader" "JPEG stream decoder present"
require_text "$ROOT_DIR/kernel/src/gui.cpp" "DecodeSvgDocument" "SVG parser present"
require_text "$ROOT_DIR/kernel/src/gui.cpp" "LoadTrueTypeFont" "TrueType/OpenType loader present"
require_text "$ROOT_DIR/kernel/src/gui.cpp" "CacheFontGlyph" "font glyph cache present"
require_text "$ROOT_DIR/kernel/src/gui.cpp" "ExecuteTerminalCommand" "terminal command dispatcher present"
require_text "$ROOT_DIR/kernel/src/gui.cpp" "cat readme" "terminal cat command present"
require_text "$ROOT_DIR/kernel/src/gui.cpp" "shutdown: request queued" "terminal shutdown command present"

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
