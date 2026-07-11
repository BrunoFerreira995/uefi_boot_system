#pragma once

#include <stdint.h>

#include "../../common/boot_info.hpp"

struct GuiStatus {
    bool window_manager_ready;
    bool compositor_ready;
    bool desktop_ready;
    bool event_queue_ready;
    bool mouse_ready;
    uint32_t window_count;
    uint32_t queued_events;
};

enum class GuiEventType : uint8_t {
    None,
    MouseMove,
    MouseDown,
    MouseUp,
    MouseWheel,
    Click,
    DoubleClick,
    Hover,
    Drag,
    KeyDown,
    KeyUp,
    Gamepad,
    Paint,
    Resize,
    Close,
};

struct GuiEvent {
    GuiEventType type;
    int32_t x;
    int32_t y;
    uint32_t button;
    uint32_t key;
};

bool KernelGuiInit(const BootInfo& boot_info);
const GuiStatus& KernelGuiStatus();
bool KernelGuiPostEvent(const GuiEvent& event);
bool KernelGuiPollEvent(GuiEvent& event);
void KernelGuiPumpEvents();
void PrintGuiInfo();
