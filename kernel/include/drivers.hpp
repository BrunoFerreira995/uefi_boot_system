#pragma once

#include <stdint.h>

#include "../../common/boot_info.hpp"

struct DriverStatus {
    bool framebuffer_ready;
    bool keyboard_ready;
    bool mouse_ready;
    bool ps2_packet_decoder_ready;
    bool ps2_mouse_hardware_ready;
    bool mouse_irq12_ready;
    bool mouse_event_queue_ready;
    bool cursor_framebuffer_ready;
    bool qemu_mouse_ready;
    bool pci_ready;
    bool pci_config_ready;
    bool pci_interrupts_ready;
    bool usb_supported;
    bool usb_hid_supported;
    bool ahci_supported;
    bool nvme_supported;
    bool network_supported;
    bool ethernet_supported;
    bool e1000_supported;
    bool virtio_net_supported;
    bool wifi_supported;
    bool drm_kms_ready;
    bool gpu_memory_ready;
    bool intel_gpu_supported;
    bool amd_gpu_supported;
    bool virtio_gpu_supported;
    bool hardware_cursor_ready;
    bool double_buffering_ready;
    bool triple_buffering_ready;
    bool hda_supported;
    bool pcm_ready;
    bool audio_mixer_ready;
    bool audio_api_ready;
    bool audio32_ready;
    bool uhci_supported;
    bool ohci_supported;
    bool ehci_supported;
    bool xhci_supported;
    bool usb_mass_storage_ready;
    bool mouse_wheel_ready;
    bool relative_mouse_ready;
    bool gamepad_ready;
    bool joystick_ready;
    uint32_t pci_device_count;
    uint32_t mouse_irq12_count;
    uint32_t mouse_packet_count;
    uint32_t mouse_event_count;
    uint32_t usb_controller_count;
    uint32_t ahci_controller_count;
    uint32_t nvme_controller_count;
    uint32_t network_controller_count;
    uint32_t ethernet_controller_count;
    uint32_t wifi_controller_count;
};

bool KernelDriversInit(const BootInfo& boot_info);
const DriverStatus& KernelDriversStatus();
void PrintDriverInfo();
