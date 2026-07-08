#pragma once

#include <stdint.h>

#include "../../common/boot_info.hpp"

struct GuiStatus {
    bool window_manager_ready;
    bool compositor_ready;
    bool desktop_ready;
    uint32_t window_count;
};

bool KernelGuiInit(const BootInfo& boot_info);
const GuiStatus& KernelGuiStatus();
void PrintGuiInfo();
