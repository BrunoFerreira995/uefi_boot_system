#!/bin/bash
# Antigravity OS — Phase 3 Boot Execution Script
set -e

# Resolve paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
ESP_DIR="$ROOT_DIR/ESP"

BOOTLOADER_EFI="$BUILD_DIR/bootloader/BOOTX64.EFI"
if [ ! -f "$BOOTLOADER_EFI" ]; then
    BOOTLOADER_EFI="$BUILD_DIR/bootloader/BOOTX64.EFI.exe"
fi
KERNEL_ELF="$BUILD_DIR/kernel/kernel.elf"

# 1. Build validation and trigger
if [ ! -f "$BOOTLOADER_EFI" ] || [ ! -f "$KERNEL_ELF" ]; then
    echo "Build artifacts missing. Triggering build..."
    "$SCRIPT_DIR/build.sh"
fi

# 2. Recreate virtual ESP structure
echo "Preparing EFI System Partition directory..."
rm -rf "$ESP_DIR"
mkdir -p "$ESP_DIR/EFI/BOOT"
mkdir -p "$ESP_DIR/kernel"

cp "$BOOTLOADER_EFI" "$ESP_DIR/EFI/BOOT/BOOTX64.EFI"
cp "$KERNEL_ELF" "$ESP_DIR/kernel/kernel.elf"

# 3. Locate UEFI (OVMF) Firmware Code
echo "Locating OVMF firmware..."
OVMF_PATHS=(
    "/opt/homebrew/share/qemu/edk2-x86_64-code.fd"            # macOS Homebrew (Newer)
    "/opt/homebrew/Cellar/qemu/*/share/qemu/edk2-x86_64-code.fd" # macOS Cellar
    "/usr/share/OVMF/OVMF_CODE.fd"                           # Ubuntu/Debian
    "/usr/share/ovmf/x64/OVMF_CODE.fd"                       # Arch Linux
    "/usr/share/edk2/ovmf/OVMF_CODE.fd"                      # Fedora/RHEL
    "/usr/share/ovmf/OVMF.fd"                                # Fallback Generic Linux
)

OVMF=""
for path in "${OVMF_PATHS[@]}"; do
    # Expand glob patterns safely
    expanded=( $path )
    if [ -f "${expanded[0]}" ]; then
        OVMF="${expanded[0]}"
        break
    fi
done

if [ -z "$OVMF" ]; then
    echo "Error: OVMF (EDK2) UEFI firmware image not found."
    echo "Please install OVMF / QEMU UEFI firmware packages."
    exit 1
fi

echo "Found OVMF firmware: $OVMF"

# 4. Package Disk Image (optional, fallback to virtual FAT for maximum compatibility)
IMG_FILE="$ROOT_DIR/esp.img"
PACKAGED=false

echo "Packaging partition image..."
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS native hdiutil to create FAT32
    if hdiutil create -srcfolder "$ESP_DIR" -fs MS-DOS -volname "EFI_SYSTEM" -format UDTO "$ROOT_DIR/esp_temp.cdr" &>/dev/null; then
        mv "$ROOT_DIR/esp_temp.cdr" "$IMG_FILE"
        PACKAGED=true
        echo "Successfully created FAT32 raw image: $IMG_FILE"
    fi
elif command -v mkfs.vfat &>/dev/null && command -v mcopy &>/dev/null; then
    # Linux loopback-free image creation using dd and mtools
    dd if=/dev/zero of="$IMG_FILE" bs=1M count=64 &>/dev/null
    mkfs.vfat -F 32 -n "EFI_SYSTEM" "$IMG_FILE" &>/dev/null
    mmd -i "$IMG_FILE" ::/EFI ::/EFI/BOOT ::/kernel
    mcopy -i "$IMG_FILE" "$BOOTLOADER_EFI" ::/EFI/BOOT/BOOTX64.EFI
    mcopy -i "$IMG_FILE" "$KERNEL_ELF" ::/kernel/kernel.elf
    PACKAGED=true
    echo "Successfully created FAT32 raw image: $IMG_FILE"
fi

# 5. Execute QEMU
echo "Launching QEMU..."
if [ "$PACKAGED" = true ]; then
    # Boot from raw hard drive image file
    qemu-system-x86_64 \
        -drive if=pflash,format=raw,readonly=on,file="$OVMF" \
        -drive file="$IMG_FILE",format=raw \
        -net none \
        -m 256M \
        -vga std
else
    # Boot using QEMU virtual FAT directory mapper
    echo "Warning: Direct image packaging failed/skipped. Booting from QEMU virtual FAT folder..."
    qemu-system-x86_64 \
        -drive if=pflash,format=raw,readonly=on,file="$OVMF" \
        -drive file=fat:rw:"$ESP_DIR",format=raw \
        -net none \
        -m 256M \
        -vga std
fi
