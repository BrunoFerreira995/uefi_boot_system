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
