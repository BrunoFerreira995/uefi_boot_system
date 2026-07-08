#pragma once

#include <stdint.h>

#include "../../common/boot_info.hpp"

struct DriverStatus {
    bool framebuffer_ready;
    bool keyboard_ready;
    bool mouse_ready;
    bool pci_ready;
    bool usb_supported;
    bool nvme_supported;
    bool network_supported;
    uint32_t pci_device_count;
    uint32_t usb_controller_count;
    uint32_t nvme_controller_count;
    uint32_t network_controller_count;
};

bool KernelDriversInit(const BootInfo& boot_info);
const DriverStatus& KernelDriversStatus();
void PrintDriverInfo();
