#include "gui.hpp"

#include "kernel.hpp"

namespace {

struct Rect {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
};

struct Window {
    const char* title;
    Rect bounds;
    uint32_t color;
    bool visible;
};

static constexpr uint32_t kMaxWindows = 8;
static constexpr uint32_t kDesktopColor = 0xFF18202A;
static constexpr uint32_t kPanelColor = 0xFF202C38;
static constexpr uint32_t kTitleColor = 0xFF32485C;
static constexpr uint32_t kAccentColor = 0xFF00D084;
static constexpr uint32_t kWindowAColor = 0xFF25364A;
static constexpr uint32_t kWindowBColor = 0xFF3A3148;
static constexpr uint32_t kShadowColor = 0x66000000;

GuiStatus g_Status {};
FramebufferInfo g_Framebuffer {};
Window g_Windows[kMaxWindows];

void ResetGuiStatus() {
    g_Status.window_manager_ready = false;
    g_Status.compositor_ready = false;
    g_Status.desktop_ready = false;
    g_Status.window_count = 0;

    g_Framebuffer.base_address = 0;
    g_Framebuffer.width = 0;
    g_Framebuffer.height = 0;
    g_Framebuffer.pixels_per_scanline = 0;
    g_Framebuffer.format = 0;

    for (uint32_t i = 0; i < kMaxWindows; i++) {
        g_Windows[i].title = nullptr;
        g_Windows[i].bounds = {0, 0, 0, 0};
        g_Windows[i].color = 0;
        g_Windows[i].visible = false;
    }
}

bool FramebufferValid(const FramebufferInfo& framebuffer) {
    return framebuffer.base_address != 0 &&
        framebuffer.width >= 320 &&
        framebuffer.height >= 200 &&
        framebuffer.pixels_per_scanline >= framebuffer.width;
}

void DrawPixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= g_Framebuffer.width || y >= g_Framebuffer.height) {
        return;
    }

    uint32_t* pixels = reinterpret_cast<uint32_t*>(g_Framebuffer.base_address);
    pixels[y * g_Framebuffer.pixels_per_scanline + x] = color;
}

void FillRect(const Rect& rect, uint32_t color) {
    const uint32_t max_x = rect.x + rect.width;
    const uint32_t max_y = rect.y + rect.height;

    for (uint32_t y = rect.y; y < max_y && y < g_Framebuffer.height; y++) {
        for (uint32_t x = rect.x; x < max_x && x < g_Framebuffer.width; x++) {
            DrawPixel(x, y, color);
        }
    }
}

void DrawBorder(const Rect& rect, uint32_t color) {
    if (rect.width == 0 || rect.height == 0) {
        return;
    }

    FillRect({rect.x, rect.y, rect.width, 2}, color);
    FillRect({rect.x, rect.y + rect.height - 2, rect.width, 2}, color);
    FillRect({rect.x, rect.y, 2, rect.height}, color);
    FillRect({rect.x + rect.width - 2, rect.y, 2, rect.height}, color);
}

bool AddWindow(const char* title, Rect bounds, uint32_t color) {
    if (!title || bounds.width == 0 || bounds.height == 0 || g_Status.window_count >= kMaxWindows) {
        return false;
    }

    Window& window = g_Windows[g_Status.window_count];
    window.title = title;
    window.bounds = bounds;
    window.color = color;
    window.visible = true;
    g_Status.window_count++;
    return true;
}

bool InitWindowManager() {
    const uint32_t margin = 32;
    const uint32_t usable_width = g_Framebuffer.width > margin * 2 ? g_Framebuffer.width - margin * 2 : g_Framebuffer.width;
    const uint32_t usable_height = g_Framebuffer.height > 120 ? g_Framebuffer.height - 120 : g_Framebuffer.height;

    const Rect shell = {
        margin,
        72,
        usable_width > 520 ? 520 : usable_width,
        usable_height > 260 ? 260 : usable_height,
    };
    const Rect monitor = {
        margin + shell.width / 2,
        138,
        usable_width > 460 ? 460 : usable_width,
        usable_height > 220 ? 220 : usable_height,
    };

    g_Status.window_manager_ready =
        AddWindow("Shell", shell, kWindowAColor) &&
        AddWindow("System Monitor", monitor, kWindowBColor);
    return g_Status.window_manager_ready;
}

void ComposeWindow(const Window& window) {
    if (!window.visible) {
        return;
    }

    const Rect shadow = {
        window.bounds.x + 8,
        window.bounds.y + 8,
        window.bounds.width,
        window.bounds.height,
    };
    FillRect(shadow, kShadowColor);
    FillRect(window.bounds, window.color);
    FillRect({window.bounds.x, window.bounds.y, window.bounds.width, 26}, kTitleColor);
    DrawBorder(window.bounds, kAccentColor);
}

bool ComposeDesktop() {
    FillRect({0, 0, g_Framebuffer.width, g_Framebuffer.height}, kDesktopColor);
    FillRect({0, 0, g_Framebuffer.width, 44}, kPanelColor);
    FillRect({0, g_Framebuffer.height - 42, g_Framebuffer.width, 42}, kPanelColor);
    FillRect({24, 12, 84, 8}, kAccentColor);
    FillRect({24, g_Framebuffer.height - 27, 120, 12}, kAccentColor);

    for (uint32_t i = 0; i < g_Status.window_count; i++) {
        ComposeWindow(g_Windows[i]);
    }

    return true;
}

} // namespace

bool KernelGuiInit(const BootInfo& boot_info) {
    ResetGuiStatus();

    if (!FramebufferValid(boot_info.framebuffer)) {
        KernelLog(LogLevel::Warn, "GUI framebuffer unavailable");
        return false;
    }

    g_Framebuffer = boot_info.framebuffer;
    if (!InitWindowManager()) {
        return false;
    }

    g_Status.compositor_ready = ComposeDesktop();
    g_Status.desktop_ready = g_Status.compositor_ready && g_Status.window_manager_ready;

    KernelLog(LogLevel::Info, "Phase 10 GUI initialized");
    return g_Status.window_manager_ready && g_Status.compositor_ready && g_Status.desktop_ready;
}

const GuiStatus& KernelGuiStatus() {
    return g_Status;
}

void PrintGuiInfo() {
    KernelLog(g_Status.window_manager_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.window_manager_ready ? "Window manager ready" : "Window manager unavailable");
    KernelLog(g_Status.compositor_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.compositor_ready ? "Compositor rendered desktop" : "Compositor unavailable");
    KernelLog(g_Status.desktop_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.desktop_ready ? "Desktop environment ready" : "Desktop environment unavailable");
}
