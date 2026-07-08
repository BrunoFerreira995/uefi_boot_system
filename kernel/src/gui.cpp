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
    bool terminal;
};

static constexpr uint32_t kMaxWindows = 8;
static constexpr uint32_t kEventQueueCapacity = 64;
static constexpr uint32_t kTitleBarHeight = 26;
static constexpr uint32_t kCloseButtonSize = 16;
static constexpr uint32_t kCursorHeight = 16;
static constexpr uint32_t kDesktopColor = 0xFF18202A;
static constexpr uint32_t kPanelColor = 0xFF202C38;
static constexpr uint32_t kTitleColor = 0xFF32485C;
static constexpr uint32_t kActiveTitleColor = 0xFF3C5D78;
static constexpr uint32_t kAccentColor = 0xFF00D084;
static constexpr uint32_t kTextColor = 0xFFE8EEF4;
static constexpr uint32_t kMutedTextColor = 0xFF9FB0C0;
static constexpr uint32_t kCloseColor = 0xFFE85D75;
static constexpr uint32_t kTerminalColor = 0xFF06120F;
static constexpr uint32_t kCursorColor = 0xFFFFFFFF;
static constexpr uint32_t kCursorShadowColor = 0xFF101820;
static constexpr uint32_t kWindowAColor = 0xFF25364A;
static constexpr uint32_t kWindowBColor = 0xFF3A3148;
static constexpr uint32_t kShadowColor = 0x66000000;

GuiStatus g_Status {};
FramebufferInfo g_Framebuffer {};
Window g_Windows[kMaxWindows];
GuiEvent g_EventQueue[kEventQueueCapacity];
uint32_t g_EventReadIndex = 0;
uint32_t g_EventWriteIndex = 0;
uint32_t g_EventCount = 0;
int32_t g_MouseX = 0;
int32_t g_MouseY = 0;
bool g_LeftButtonDown = false;
int32_t g_DragWindowIndex = -1;
int32_t g_DragOffsetX = 0;
int32_t g_DragOffsetY = 0;
int32_t g_MouseDownWindowIndex = -1;
int32_t g_MouseDownX = 0;
int32_t g_MouseDownY = 0;
bool g_Dragging = false;

uint32_t ConvertColor(uint32_t color) {
    uint32_t red = (color >> 16) & 0xFF;
    uint32_t green = (color >> 8) & 0xFF;
    uint32_t blue = color & 0xFF;

    if (g_Framebuffer.format == 0) {
        return red | (green << 8) | (blue << 16);
    }

    return blue | (green << 8) | (red << 16);
}

void ResetGuiStatus() {
    g_Status.window_manager_ready = false;
    g_Status.compositor_ready = false;
    g_Status.desktop_ready = false;
    g_Status.event_queue_ready = false;
    g_Status.mouse_ready = false;
    g_Status.window_count = 0;
    g_Status.queued_events = 0;

    g_Framebuffer.base_address = 0;
    g_Framebuffer.width = 0;
    g_Framebuffer.height = 0;
    g_Framebuffer.pixels_per_scanline = 0;
    g_Framebuffer.format = 0;

    g_EventReadIndex = 0;
    g_EventWriteIndex = 0;
    g_EventCount = 0;
    g_MouseX = 0;
    g_MouseY = 0;
    g_LeftButtonDown = false;
    g_DragWindowIndex = -1;
    g_DragOffsetX = 0;
    g_DragOffsetY = 0;
    g_MouseDownWindowIndex = -1;
    g_MouseDownX = 0;
    g_MouseDownY = 0;
    g_Dragging = false;

    for (uint32_t i = 0; i < kMaxWindows; i++) {
        g_Windows[i].title = nullptr;
        g_Windows[i].bounds = {0, 0, 0, 0};
        g_Windows[i].color = 0;
        g_Windows[i].visible = false;
        g_Windows[i].terminal = false;
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
    pixels[y * g_Framebuffer.pixels_per_scanline + x] = ConvertColor(color);
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

uint8_t GlyphRow(char c, uint32_t row) {
    if (c >= 'a' && c <= 'z') {
        c = static_cast<char>(c - 'a' + 'A');
    }

    switch (c) {
        case 'A': { static constexpr uint8_t r[7] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}; return r[row]; }
        case 'B': { static constexpr uint8_t r[7] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}; return r[row]; }
        case 'C': { static constexpr uint8_t r[7] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}; return r[row]; }
        case 'D': { static constexpr uint8_t r[7] = {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}; return r[row]; }
        case 'E': { static constexpr uint8_t r[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}; return r[row]; }
        case 'F': { static constexpr uint8_t r[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}; return r[row]; }
        case 'G': { static constexpr uint8_t r[7] = {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}; return r[row]; }
        case 'H': { static constexpr uint8_t r[7] = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}; return r[row]; }
        case 'I': { static constexpr uint8_t r[7] = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}; return r[row]; }
        case 'J': { static constexpr uint8_t r[7] = {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C}; return r[row]; }
        case 'K': { static constexpr uint8_t r[7] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}; return r[row]; }
        case 'L': { static constexpr uint8_t r[7] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}; return r[row]; }
        case 'M': { static constexpr uint8_t r[7] = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}; return r[row]; }
        case 'N': { static constexpr uint8_t r[7] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}; return r[row]; }
        case 'O': { static constexpr uint8_t r[7] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}; return r[row]; }
        case 'P': { static constexpr uint8_t r[7] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}; return r[row]; }
        case 'Q': { static constexpr uint8_t r[7] = {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}; return r[row]; }
        case 'R': { static constexpr uint8_t r[7] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}; return r[row]; }
        case 'S': { static constexpr uint8_t r[7] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}; return r[row]; }
        case 'T': { static constexpr uint8_t r[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}; return r[row]; }
        case 'U': { static constexpr uint8_t r[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}; return r[row]; }
        case 'V': { static constexpr uint8_t r[7] = {0x11, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04}; return r[row]; }
        case 'W': { static constexpr uint8_t r[7] = {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}; return r[row]; }
        case 'X': { static constexpr uint8_t r[7] = {0x11, 0x0A, 0x04, 0x04, 0x04, 0x0A, 0x11}; return r[row]; }
        case 'Y': { static constexpr uint8_t r[7] = {0x11, 0x0A, 0x04, 0x04, 0x04, 0x04, 0x04}; return r[row]; }
        case 'Z': { static constexpr uint8_t r[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}; return r[row]; }
        case '0': { static constexpr uint8_t r[7] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}; return r[row]; }
        case '1': { static constexpr uint8_t r[7] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}; return r[row]; }
        case '2': { static constexpr uint8_t r[7] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}; return r[row]; }
        case '3': { static constexpr uint8_t r[7] = {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}; return r[row]; }
        case '4': { static constexpr uint8_t r[7] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}; return r[row]; }
        case '5': { static constexpr uint8_t r[7] = {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E}; return r[row]; }
        case '6': { static constexpr uint8_t r[7] = {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E}; return r[row]; }
        case '7': { static constexpr uint8_t r[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}; return r[row]; }
        case '8': { static constexpr uint8_t r[7] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}; return r[row]; }
        case '9': { static constexpr uint8_t r[7] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E}; return r[row]; }
        case '.': { static constexpr uint8_t r[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}; return r[row]; }
        case ':': { static constexpr uint8_t r[7] = {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00}; return r[row]; }
        case '%': { static constexpr uint8_t r[7] = {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03}; return r[row]; }
        case '>': { static constexpr uint8_t r[7] = {0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10}; return r[row]; }
        case '-': { static constexpr uint8_t r[7] = {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}; return r[row]; }
        case ' ': return 0x00;
        default: { static constexpr uint8_t r[7] = {0x1F, 0x01, 0x02, 0x04, 0x00, 0x04, 0x00}; return r[row]; }
    }
}

namespace Graphics {

void FillRect(const Rect& rect, uint32_t color) {
    const uint32_t max_x = rect.x + rect.width;
    const uint32_t max_y = rect.y + rect.height;

    for (uint32_t y = rect.y; y < max_y && y < g_Framebuffer.height; y++) {
        for (uint32_t x = rect.x; x < max_x && x < g_Framebuffer.width; x++) {
            DrawPixel(x, y, color);
        }
    }
}

void DrawRect(const Rect& rect, uint32_t color) {
    if (rect.width == 0 || rect.height == 0) {
        return;
    }

    FillRect({rect.x, rect.y, rect.width, 2}, color);
    FillRect({rect.x, rect.y + rect.height - 2, rect.width, 2}, color);
    FillRect({rect.x, rect.y, 2, rect.height}, color);
    FillRect({rect.x + rect.width - 2, rect.y, 2, rect.height}, color);
}

void DrawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    const int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1;
    const int32_t sx = x0 < x1 ? 1 : -1;
    const int32_t dy = y1 > y0 ? y0 - y1 : y1 - y0;
    const int32_t sy = y0 < y1 ? 1 : -1;
    int32_t err = dx + dy;

    while (true) {
        if (x0 >= 0 && y0 >= 0) {
            DrawPixel(static_cast<uint32_t>(x0), static_cast<uint32_t>(y0), color);
        }
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int32_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void DrawChar(uint32_t x, uint32_t y, char c, uint32_t color) {
    for (uint32_t row = 0; row < 7; row++) {
        const uint8_t bits = GlyphRow(c, row);
        for (uint32_t col = 0; col < 5; col++) {
            if ((bits & (1u << (4 - col))) != 0) {
                FillRect({x + col * 2, y + row * 2, 2, 2}, color);
            }
        }
    }
}

void DrawText(uint32_t x, uint32_t y, const char* text, uint32_t color) {
    if (!text) {
        return;
    }

    uint32_t cursor_x = x;
    while (*text) {
        DrawChar(cursor_x, y, *text, color);
        cursor_x += 12;
        text++;
    }
}

void DrawImage(const Rect& rect, uint32_t color) {
    FillRect(rect, color);
}

} // namespace Graphics

void DrawCursor() {
    const uint32_t x = g_MouseX < 0 ? 0 : static_cast<uint32_t>(g_MouseX);
    const uint32_t y = g_MouseY < 0 ? 0 : static_cast<uint32_t>(g_MouseY);

    FillRect({x + 2, y + 2, 2, kCursorHeight}, kCursorShadowColor);
    FillRect({x + 4, y + 10, 2, 6}, kCursorShadowColor);
    FillRect({x, y, 2, kCursorHeight}, kCursorColor);
    FillRect({x + 2, y + 2, 2, 12}, kCursorColor);
    FillRect({x + 4, y + 4, 2, 8}, kCursorColor);
    FillRect({x + 6, y + 6, 2, 4}, kCursorColor);
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

bool PointInRect(int32_t x, int32_t y, const Rect& rect) {
    return x >= static_cast<int32_t>(rect.x) &&
        y >= static_cast<int32_t>(rect.y) &&
        x < static_cast<int32_t>(rect.x + rect.width) &&
        y < static_cast<int32_t>(rect.y + rect.height);
}

bool PointInTitleBar(int32_t x, int32_t y, const Window& window) {
    return PointInRect(x, y, {window.bounds.x, window.bounds.y, window.bounds.width, kTitleBarHeight});
}

Rect CloseButtonRect(const Window& window) {
    const uint32_t x = window.bounds.x + window.bounds.width - kCloseButtonSize - 8;
    const uint32_t y = window.bounds.y + (kTitleBarHeight - kCloseButtonSize) / 2;
    return {x, y, kCloseButtonSize, kCloseButtonSize};
}

bool PointInCloseButton(int32_t x, int32_t y, const Window& window) {
    return PointInRect(x, y, CloseButtonRect(window));
}

int32_t FindTopWindowAt(int32_t x, int32_t y, bool title_bar_only) {
    for (int32_t i = static_cast<int32_t>(g_Status.window_count) - 1; i >= 0; i--) {
        const Window& window = g_Windows[i];
        if (!window.visible) {
            continue;
        }

        if (title_bar_only ? PointInTitleBar(x, y, window) : PointInRect(x, y, window.bounds)) {
            return i;
        }
    }

    return -1;
}

void BringWindowToFront(uint32_t index) {
    if (index >= g_Status.window_count || index + 1 == g_Status.window_count) {
        return;
    }

    Window selected = g_Windows[index];
    for (uint32_t i = index; i + 1 < g_Status.window_count; i++) {
        g_Windows[i] = g_Windows[i + 1];
    }
    g_Windows[g_Status.window_count - 1] = selected;
    g_DragWindowIndex = static_cast<int32_t>(g_Status.window_count - 1);
}

void MoveWindow(Window& window, int32_t x, int32_t y) {
    const int32_t max_x = g_Framebuffer.width > window.bounds.width
        ? static_cast<int32_t>(g_Framebuffer.width - window.bounds.width)
        : 0;
    const int32_t max_y = g_Framebuffer.height > window.bounds.height
        ? static_cast<int32_t>(g_Framebuffer.height - window.bounds.height)
        : 0;

    if (x < 0) {
        x = 0;
    } else if (x > max_x) {
        x = max_x;
    }

    if (y < static_cast<int32_t>(44)) {
        y = 44;
    } else if (y > max_y) {
        y = max_y;
    }

    window.bounds.x = static_cast<uint32_t>(x);
    window.bounds.y = static_cast<uint32_t>(y);
}

bool AddWindow(const char* title, Rect bounds, uint32_t color, bool terminal) {
    if (!title || bounds.width == 0 || bounds.height == 0 || g_Status.window_count >= kMaxWindows) {
        return false;
    }

    Window& window = g_Windows[g_Status.window_count];
    window.title = title;
    window.bounds = bounds;
    window.color = color;
    window.visible = true;
    window.terminal = terminal;
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
        AddWindow("Terminal", shell, kTerminalColor, true) &&
        AddWindow("System Monitor", monitor, kWindowBColor, false);
    return g_Status.window_manager_ready;
}

void DrawTerminalContents(const Window& window) {
    const uint32_t x = window.bounds.x + 18;
    uint32_t y = window.bounds.y + kTitleBarHeight + 18;

    Graphics::DrawText(x, y, "AntigravityOS v0.1", kTextColor);
    y += 24;
    Graphics::DrawText(x, y, "> help", kAccentColor);
    y += 24;
    Graphics::DrawText(x, y, "help mem cpu clear", kMutedTextColor);
    y += 20;
    Graphics::DrawText(x, y, "version uptime reboot", kMutedTextColor);
    y += 32;
    Graphics::DrawText(x, y, "> ", kAccentColor);
}

void DrawSystemMonitorContents(const Window& window) {
    const uint32_t x = window.bounds.x + 18;
    uint32_t y = window.bounds.y + kTitleBarHeight + 18;

    Graphics::DrawText(x, y, "CPU 3%", kTextColor);
    y += 24;
    Graphics::DrawText(x, y, "RAM 12 MB", kTextColor);
    y += 24;
    Graphics::DrawText(x, y, "FPS 60", kTextColor);
    y += 28;
    Graphics::FillRect({x, y, 150, 10}, 0xFF1B2632);
    Graphics::FillRect({x, y, 48, 10}, kAccentColor);
}

void ComposeWindow(const Window& window, bool active) {
    if (!window.visible) {
        return;
    }

    const Rect shadow = {
        window.bounds.x + 8,
        window.bounds.y + 8,
        window.bounds.width,
        window.bounds.height,
    };
    Graphics::FillRect(shadow, kShadowColor);
    Graphics::FillRect(window.bounds, window.color);
    Graphics::FillRect({window.bounds.x, window.bounds.y, window.bounds.width, kTitleBarHeight},
        active ? kActiveTitleColor : kTitleColor);
    Graphics::DrawText(window.bounds.x + 12, window.bounds.y + 6, window.title, kTextColor);

    const Rect close = CloseButtonRect(window);
    Graphics::FillRect(close, kCloseColor);
    Graphics::DrawText(close.x + 4, close.y + 2, "X", 0xFFFFFFFF);
    Graphics::DrawRect(window.bounds, active ? kAccentColor : 0xFF597082);

    if (window.terminal) {
        DrawTerminalContents(window);
    } else {
        DrawSystemMonitorContents(window);
    }
}

bool ComposeDesktop() {
    Graphics::FillRect({0, 0, g_Framebuffer.width, g_Framebuffer.height}, kDesktopColor);
    Graphics::FillRect({0, 0, g_Framebuffer.width, 44}, kPanelColor);
    Graphics::FillRect({0, g_Framebuffer.height - 42, g_Framebuffer.width, 42}, kPanelColor);
    Graphics::DrawText(24, 14, "Antigravity OS", kTextColor);
    Graphics::DrawText(200, 14, "CPU 3%", kMutedTextColor);
    Graphics::DrawText(292, 14, "RAM 12 MB", kMutedTextColor);
    Graphics::DrawText(420, 14, "FPS 60", kMutedTextColor);
    if (g_Framebuffer.width > 92) {
        Graphics::DrawText(g_Framebuffer.width - 82, 14, "22:45", kTextColor);
    }
    Graphics::DrawText(24, g_Framebuffer.height - 27, "Files", kAccentColor);

    int32_t active_window = -1;
    for (int32_t i = static_cast<int32_t>(g_Status.window_count) - 1; i >= 0; i--) {
        if (g_Windows[i].visible) {
            active_window = i;
            break;
        }
    }

    for (uint32_t i = 0; i < g_Status.window_count; i++) {
        ComposeWindow(g_Windows[i], static_cast<int32_t>(i) == active_window);
    }

    DrawCursor();
    return true;
}

void ClampMouseToScreen() {
    if (g_MouseX < 0) {
        g_MouseX = 0;
    } else if (g_MouseX >= static_cast<int32_t>(g_Framebuffer.width)) {
        g_MouseX = static_cast<int32_t>(g_Framebuffer.width - 1);
    }

    if (g_MouseY < 0) {
        g_MouseY = 0;
    } else if (g_MouseY >= static_cast<int32_t>(g_Framebuffer.height)) {
        g_MouseY = static_cast<int32_t>(g_Framebuffer.height - 1);
    }
}

void CloseWindow(uint32_t index) {
    if (index >= g_Status.window_count) {
        return;
    }

    g_Windows[index].visible = false;
    if (g_DragWindowIndex == static_cast<int32_t>(index)) {
        g_DragWindowIndex = -1;
    }
    if (g_MouseDownWindowIndex == static_cast<int32_t>(index)) {
        g_MouseDownWindowIndex = -1;
    }
}

void HandleMouseClick(int32_t x, int32_t y, uint32_t button) {
    if (button != 0) {
        return;
    }

    const int32_t window_index = FindTopWindowAt(x, y, false);
    if (window_index < 0) {
        return;
    }

    Window& window = g_Windows[window_index];
    if (PointInCloseButton(x, y, window)) {
        CloseWindow(static_cast<uint32_t>(window_index));
    }
}

void DispatchEvent(const GuiEvent& event) {
    switch (event.type) {
        case GuiEventType::MouseMove:
            g_MouseX = event.x;
            g_MouseY = event.y;
            ClampMouseToScreen();
            if (g_LeftButtonDown && g_DragWindowIndex >= 0) {
                g_Dragging = true;
                MoveWindow(g_Windows[g_DragWindowIndex], g_MouseX - g_DragOffsetX, g_MouseY - g_DragOffsetY);
            }
            ComposeDesktop();
            break;

        case GuiEventType::MouseDown:
            g_LeftButtonDown = event.button == 0;
            g_MouseX = event.x;
            g_MouseY = event.y;
            ClampMouseToScreen();
            if (g_LeftButtonDown) {
                g_MouseDownX = g_MouseX;
                g_MouseDownY = g_MouseY;
                g_MouseDownWindowIndex = FindTopWindowAt(g_MouseX, g_MouseY, false);
                g_DragWindowIndex = FindTopWindowAt(g_MouseX, g_MouseY, true);
                g_Dragging = false;

                const int32_t front_window = g_MouseDownWindowIndex;
                if (front_window >= 0) {
                    BringWindowToFront(static_cast<uint32_t>(front_window));
                    if (g_DragWindowIndex == front_window) {
                        g_DragWindowIndex = static_cast<int32_t>(g_Status.window_count - 1);
                    }
                }

                if (g_DragWindowIndex >= 0) {
                    Window& window = g_Windows[g_DragWindowIndex];
                    g_DragOffsetX = g_MouseX - static_cast<int32_t>(window.bounds.x);
                    g_DragOffsetY = g_MouseY - static_cast<int32_t>(window.bounds.y);
                }
            }
            ComposeDesktop();
            break;

        case GuiEventType::MouseUp:
            g_MouseX = event.x;
            g_MouseY = event.y;
            ClampMouseToScreen();
            if (g_LeftButtonDown && !g_Dragging) {
                const int32_t dx = g_MouseX - g_MouseDownX;
                const int32_t dy = g_MouseY - g_MouseDownY;
                if (dx > -3 && dx < 3 && dy > -3 && dy < 3) {
                    HandleMouseClick(g_MouseX, g_MouseY, event.button);
                }
            }
            g_LeftButtonDown = false;
            g_DragWindowIndex = -1;
            g_MouseDownWindowIndex = -1;
            g_Dragging = false;
            ComposeDesktop();
            break;

        case GuiEventType::Click:
            g_MouseX = event.x;
            g_MouseY = event.y;
            ClampMouseToScreen();
            HandleMouseClick(g_MouseX, g_MouseY, event.button);
            ComposeDesktop();
            break;

        case GuiEventType::Drag:
            if (g_LeftButtonDown && g_DragWindowIndex >= 0) {
                g_MouseX = event.x;
                g_MouseY = event.y;
                ClampMouseToScreen();
                g_Dragging = true;
                MoveWindow(g_Windows[g_DragWindowIndex], g_MouseX - g_DragOffsetX, g_MouseY - g_DragOffsetY);
                ComposeDesktop();
            }
            break;

        case GuiEventType::Paint:
        case GuiEventType::Resize:
            ComposeDesktop();
            break;

        case GuiEventType::None:
        case GuiEventType::DoubleClick:
        case GuiEventType::Hover:
        case GuiEventType::KeyDown:
        case GuiEventType::KeyUp:
        case GuiEventType::Close:
            break;
    }
}

} // namespace

bool KernelGuiInit(const BootInfo& boot_info) {
    ResetGuiStatus();

    if (!FramebufferValid(boot_info.framebuffer)) {
        KernelLog(LogLevel::Warn, "GUI framebuffer unavailable");
        return false;
    }

    g_Framebuffer = boot_info.framebuffer;
    g_MouseX = static_cast<int32_t>(g_Framebuffer.width / 2);
    g_MouseY = static_cast<int32_t>(g_Framebuffer.height / 2);
    g_Status.event_queue_ready = true;
    g_Status.mouse_ready = true;

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

bool KernelGuiPostEvent(const GuiEvent& event) {
    if (!g_Status.event_queue_ready || g_EventCount >= kEventQueueCapacity) {
        return false;
    }

    g_EventQueue[g_EventWriteIndex] = event;
    g_EventWriteIndex = (g_EventWriteIndex + 1) % kEventQueueCapacity;
    g_EventCount++;
    g_Status.queued_events = g_EventCount;
    return true;
}

bool KernelGuiPollEvent(GuiEvent& event) {
    if (!g_Status.event_queue_ready || g_EventCount == 0) {
        return false;
    }

    event = g_EventQueue[g_EventReadIndex];
    g_EventReadIndex = (g_EventReadIndex + 1) % kEventQueueCapacity;
    g_EventCount--;
    g_Status.queued_events = g_EventCount;
    return true;
}

void KernelGuiPumpEvents() {
    GuiEvent event;
    while (KernelGuiPollEvent(event)) {
        DispatchEvent(event);
    }
}

void PrintGuiInfo() {
    KernelLog(g_Status.window_manager_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.window_manager_ready ? "Window manager ready" : "Window manager unavailable");
    KernelLog(g_Status.compositor_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.compositor_ready ? "Compositor rendered desktop" : "Compositor unavailable");
    KernelLog(g_Status.desktop_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.desktop_ready ? "Desktop environment ready" : "Desktop environment unavailable");
    KernelLog(g_Status.event_queue_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.event_queue_ready ? "GUI event queue ready" : "GUI event queue unavailable");
    KernelLog(g_Status.mouse_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.mouse_ready ? "GUI mouse state ready" : "GUI mouse state unavailable");
}
