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
    Rect restore_bounds;
    uint32_t color;
    bool visible;
    bool minimized;
    bool maximized;
    bool terminal;
};

static constexpr uint32_t kMaxWindows = 8;
static constexpr uint32_t kEventQueueCapacity = 64;
static constexpr uint32_t kTopBarHeight = 38;
static constexpr uint32_t kTaskBarHeight = 42;
static constexpr uint32_t kTitleBarHeight = 30;
static constexpr uint32_t kWindowBorder = 2;
static constexpr uint32_t kButtonSize = 16;
static constexpr uint32_t kButtonGap = 7;
static constexpr uint32_t kCursorHeight = 16;
static constexpr uint32_t kCursorBackupSize = 20;
static constexpr uint32_t kBackBufferMaxWidth = 1024;
static constexpr uint32_t kBackBufferMaxHeight = 768;
static constexpr uint32_t kBackBufferPixels = kBackBufferMaxWidth * kBackBufferMaxHeight;

struct CursorBackup {
    int32_t x;
    int32_t y;
    uint32_t pixels[kCursorBackupSize * kCursorBackupSize];
    bool valid;
};

struct Theme {
    uint32_t desktop_top;
    uint32_t desktop_bottom;
    uint32_t panel;
    uint32_t panel_edge;
    uint32_t window;
    uint32_t terminal;
    uint32_t monitor;
    uint32_t title_active;
    uint32_t title_inactive;
    uint32_t border_active;
    uint32_t border_inactive;
    uint32_t button_idle;
    uint32_t button_hover;
    uint32_t close;
    uint32_t minimize;
    uint32_t maximize;
    uint32_t text;
    uint32_t text_muted;
    uint32_t accent;
    uint32_t shadow_near;
    uint32_t shadow_mid;
    uint32_t shadow_far;
    uint32_t cursor;
    uint32_t cursor_shadow;
};

static constexpr Theme kTheme = {
    0xFF202733,
    0xFF151B24,
    0xFF222B36,
    0xFF303C49,
    0xFF253241,
    0xFF07120F,
    0xFF342D46,
    0xFF344D63,
    0xFF2B3947,
    0xFF6DB7D8,
    0xFF52687A,
    0xFF3A4652,
    0xFF4A5C6B,
    0xFFE25E6F,
    0xFFE1B955,
    0xFF61C083,
    0xFFE8EEF4,
    0xFF9FB0C0,
    0xFF66D9A2,
    0xFF111821,
    0xFF0C1118,
    0xFF080B10,
    0xFFFFFFFF,
    0xFF101820,
};

enum class WindowControl : uint8_t {
    None,
    Close,
    Minimize,
    Maximize,
};

enum class CursorKind : uint8_t {
    Arrow,
    Hand,
    Move,
    Resize,
};

struct TerminalLine {
    const char* text;
    uint32_t color;
};

static constexpr uint32_t kMaxTerminalLines = 12;

GuiStatus g_Status {};
FramebufferInfo g_Framebuffer {};
Window g_Windows[kMaxWindows];
GuiEvent g_EventQueue[kEventQueueCapacity];
uint32_t g_BackBuffer[kBackBufferPixels];
uint32_t* g_DrawTarget = nullptr;
uint32_t g_DrawStride = 0;
Rect g_ClipRect {};
bool g_ClipEnabled = false;
bool g_BackBufferReady = false;
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
WindowControl g_HoveredControl = WindowControl::None;
int32_t g_HoveredWindowIndex = -1;
CursorKind g_CursorKind = CursorKind::Arrow;
CursorBackup g_CursorBackup {};
TerminalLine g_TerminalLines[kMaxTerminalLines];
uint32_t g_TerminalLineCount = 0;
const char* g_TerminalCwd = "/";
bool g_TerminalRebootRequested = false;
bool g_TerminalShutdownRequested = false;

bool PointInRect(int32_t x, int32_t y, const Rect& rect);

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
    g_DrawTarget = nullptr;
    g_DrawStride = 0;
    g_ClipRect = {0, 0, 0, 0};
    g_ClipEnabled = false;
    g_BackBufferReady = false;
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
        g_Windows[i].restore_bounds = {0, 0, 0, 0};
        g_Windows[i].color = 0;
        g_Windows[i].visible = false;
        g_Windows[i].minimized = false;
        g_Windows[i].maximized = false;
        g_Windows[i].terminal = false;
    }

    g_HoveredControl = WindowControl::None;
    g_HoveredWindowIndex = -1;
    g_CursorKind = CursorKind::Arrow;
    g_CursorBackup.x = 0;
    g_CursorBackup.y = 0;
    g_CursorBackup.valid = false;
    g_TerminalLineCount = 0;
    g_TerminalCwd = "/";
    g_TerminalRebootRequested = false;
    g_TerminalShutdownRequested = false;
    for (uint32_t i = 0; i < kMaxTerminalLines; i++) {
        g_TerminalLines[i].text = nullptr;
        g_TerminalLines[i].color = 0;
    }
}

bool FramebufferValid(const FramebufferInfo& framebuffer) {
    return framebuffer.base_address != 0 &&
        framebuffer.width >= 320 &&
        framebuffer.height >= 200 &&
        framebuffer.pixels_per_scanline >= framebuffer.width;
}

uint32_t* ActivePixels() {
    return g_DrawTarget ? g_DrawTarget : reinterpret_cast<uint32_t*>(g_Framebuffer.base_address);
}

uint32_t ActiveStride() {
    return g_DrawTarget ? g_DrawStride : g_Framebuffer.pixels_per_scanline;
}

bool RectEmpty(const Rect& rect) {
    return rect.width == 0 || rect.height == 0;
}

Rect ScreenRect() {
    return {0, 0, g_Framebuffer.width, g_Framebuffer.height};
}

Rect IntersectRect(const Rect& a, const Rect& b) {
    const uint32_t ax2 = a.x + a.width;
    const uint32_t ay2 = a.y + a.height;
    const uint32_t bx2 = b.x + b.width;
    const uint32_t by2 = b.y + b.height;
    const uint32_t x1 = a.x > b.x ? a.x : b.x;
    const uint32_t y1 = a.y > b.y ? a.y : b.y;
    const uint32_t x2 = ax2 < bx2 ? ax2 : bx2;
    const uint32_t y2 = ay2 < by2 ? ay2 : by2;

    if (x2 <= x1 || y2 <= y1) {
        return {0, 0, 0, 0};
    }
    return {x1, y1, x2 - x1, y2 - y1};
}

Rect UnionRect(const Rect& a, const Rect& b) {
    if (RectEmpty(a)) {
        return b;
    }
    if (RectEmpty(b)) {
        return a;
    }

    const uint32_t ax2 = a.x + a.width;
    const uint32_t ay2 = a.y + a.height;
    const uint32_t bx2 = b.x + b.width;
    const uint32_t by2 = b.y + b.height;
    const uint32_t x1 = a.x < b.x ? a.x : b.x;
    const uint32_t y1 = a.y < b.y ? a.y : b.y;
    const uint32_t x2 = ax2 > bx2 ? ax2 : bx2;
    const uint32_t y2 = ay2 > by2 ? ay2 : by2;
    return {x1, y1, x2 - x1, y2 - y1};
}

Rect ExpandRect(const Rect& rect, uint32_t amount) {
    const uint32_t x = rect.x > amount ? rect.x - amount : 0;
    const uint32_t y = rect.y > amount ? rect.y - amount : 0;
    const uint32_t x2 = rect.x + rect.width + amount;
    const uint32_t y2 = rect.y + rect.height + amount;
    const uint32_t max_x = x2 < g_Framebuffer.width ? x2 : g_Framebuffer.width;
    const uint32_t max_y = y2 < g_Framebuffer.height ? y2 : g_Framebuffer.height;
    return max_x > x && max_y > y ? Rect{x, y, max_x - x, max_y - y} : Rect{0, 0, 0, 0};
}

Rect CursorRectAt(int32_t x, int32_t y) {
    const uint32_t cx = x < 0 ? 0 : static_cast<uint32_t>(x);
    const uint32_t cy = y < 0 ? 0 : static_cast<uint32_t>(y);
    return IntersectRect({cx, cy, kCursorBackupSize, kCursorBackupSize}, ScreenRect());
}

Rect WindowVisualRect(const Window& window) {
    if (!window.visible || window.minimized) {
        return {0, 0, 0, 0};
    }
    return ExpandRect(window.bounds, 18);
}

void BeginDrawToFramebuffer() {
    g_DrawTarget = nullptr;
    g_DrawStride = 0;
    g_ClipEnabled = false;
}

void BeginDrawToBackBuffer(const Rect& clip) {
    g_DrawTarget = g_BackBuffer;
    g_DrawStride = kBackBufferMaxWidth;
    g_ClipRect = IntersectRect(clip, ScreenRect());
    g_ClipEnabled = true;
}

void DrawPixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= g_Framebuffer.width || y >= g_Framebuffer.height) {
        return;
    }
    if (g_ClipEnabled && !PointInRect(static_cast<int32_t>(x), static_cast<int32_t>(y), g_ClipRect)) {
        return;
    }

    uint32_t* pixels = ActivePixels();
    pixels[y * ActiveStride() + x] = ConvertColor(color);
}

uint32_t ReadRawPixel(uint32_t x, uint32_t y) {
    if (x >= g_Framebuffer.width || y >= g_Framebuffer.height) {
        return 0;
    }

    const uint32_t* pixels = reinterpret_cast<const uint32_t*>(g_Framebuffer.base_address);
    return pixels[y * g_Framebuffer.pixels_per_scanline + x];
}

void WriteRawPixel(uint32_t x, uint32_t y, uint32_t value) {
    if (x >= g_Framebuffer.width || y >= g_Framebuffer.height) {
        return;
    }

    uint32_t* pixels = reinterpret_cast<uint32_t*>(g_Framebuffer.base_address);
    pixels[y * g_Framebuffer.pixels_per_scanline + x] = value;
}

void CopyBackBufferToFramebuffer(const Rect& dirty) {
    const Rect clipped = IntersectRect(dirty, ScreenRect());
    if (!g_BackBufferReady || RectEmpty(clipped)) {
        return;
    }

    uint32_t* framebuffer = reinterpret_cast<uint32_t*>(g_Framebuffer.base_address);
    for (uint32_t y = clipped.y; y < clipped.y + clipped.height; y++) {
        for (uint32_t x = clipped.x; x < clipped.x + clipped.width; x++) {
            framebuffer[y * g_Framebuffer.pixels_per_scanline + x] = g_BackBuffer[y * kBackBufferMaxWidth + x];
        }
    }
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
    Graphics::FillRect(rect, color);
}

} // namespace Graphics

void DrawCursor() {
    const uint32_t x = g_MouseX < 0 ? 0 : static_cast<uint32_t>(g_MouseX);
    const uint32_t y = g_MouseY < 0 ? 0 : static_cast<uint32_t>(g_MouseY);

    if (g_CursorKind == CursorKind::Hand) {
        FillRect({x + 3, y + 2, 4, 14}, kTheme.cursor_shadow);
        FillRect({x + 1, y + 6, 12, 8}, kTheme.cursor_shadow);
        FillRect({x + 2, y + 1, 4, 14}, kTheme.cursor);
        FillRect({x, y + 5, 12, 8}, kTheme.cursor);
        return;
    }

    if (g_CursorKind == CursorKind::Move) {
        Graphics::DrawLine(static_cast<int32_t>(x), static_cast<int32_t>(y + 8), static_cast<int32_t>(x + 16), static_cast<int32_t>(y + 8), kTheme.cursor);
        Graphics::DrawLine(static_cast<int32_t>(x + 8), static_cast<int32_t>(y), static_cast<int32_t>(x + 8), static_cast<int32_t>(y + 16), kTheme.cursor);
        FillRect({x + 7, y + 7, 3, 3}, kTheme.cursor);
        return;
    }

    FillRect({x + 2, y + 2, 2, kCursorHeight}, kTheme.cursor_shadow);
    FillRect({x + 4, y + 10, 2, 6}, kTheme.cursor_shadow);
    FillRect({x, y, 2, kCursorHeight}, kTheme.cursor);
    FillRect({x + 2, y + 2, 2, 12}, kTheme.cursor);
    FillRect({x + 4, y + 4, 2, 8}, kTheme.cursor);
    FillRect({x + 6, y + 6, 2, 4}, kTheme.cursor);
}

void ClampMouseToScreen();
void UpdateHoverState();

void SaveCursorBackground() {
    g_CursorBackup.x = g_MouseX;
    g_CursorBackup.y = g_MouseY;
    g_CursorBackup.valid = true;

    for (uint32_t py = 0; py < kCursorBackupSize; py++) {
        for (uint32_t px = 0; px < kCursorBackupSize; px++) {
            const int32_t sx = g_CursorBackup.x + static_cast<int32_t>(px);
            const int32_t sy = g_CursorBackup.y + static_cast<int32_t>(py);
            g_CursorBackup.pixels[py * kCursorBackupSize + px] =
                (sx >= 0 && sy >= 0) ? ReadRawPixel(static_cast<uint32_t>(sx), static_cast<uint32_t>(sy)) : 0;
        }
    }
}

void RestoreCursorBackground() {
    if (!g_CursorBackup.valid) {
        return;
    }

    for (uint32_t py = 0; py < kCursorBackupSize; py++) {
        for (uint32_t px = 0; px < kCursorBackupSize; px++) {
            const int32_t sx = g_CursorBackup.x + static_cast<int32_t>(px);
            const int32_t sy = g_CursorBackup.y + static_cast<int32_t>(py);
            if (sx >= 0 && sy >= 0) {
                WriteRawPixel(static_cast<uint32_t>(sx), static_cast<uint32_t>(sy),
                    g_CursorBackup.pixels[py * kCursorBackupSize + px]);
            }
        }
    }

    g_CursorBackup.valid = false;
}

void PaintCursor() {
    SaveCursorBackground();
    DrawCursor();
}

void MoveCursorOnly(int32_t x, int32_t y) {
    RestoreCursorBackground();
    g_MouseX = x;
    g_MouseY = y;
    ClampMouseToScreen();
    UpdateHoverState();
    PaintCursor();
}

bool PointInRect(int32_t x, int32_t y, const Rect& rect) {
    return x >= static_cast<int32_t>(rect.x) &&
        y >= static_cast<int32_t>(rect.y) &&
        x < static_cast<int32_t>(rect.x + rect.width) &&
        y < static_cast<int32_t>(rect.y + rect.height);
}

void CopyWindow(Window& destination, const Window& source) {
    destination.title = source.title;
    destination.bounds = source.bounds;
    destination.restore_bounds = source.restore_bounds;
    destination.color = source.color;
    destination.visible = source.visible;
    destination.minimized = source.minimized;
    destination.maximized = source.maximized;
    destination.terminal = source.terminal;
}

bool PointInTitleBar(int32_t x, int32_t y, const Window& window) {
    return PointInRect(x, y, {window.bounds.x, window.bounds.y, window.bounds.width, kTitleBarHeight});
}

Rect ControlButtonRect(const Window& window, WindowControl control) {
    uint32_t slot = 0;
    if (control == WindowControl::Maximize) {
        slot = 1;
    } else if (control == WindowControl::Minimize) {
        slot = 2;
    }

    const uint32_t x = window.bounds.x + window.bounds.width - 9 - kButtonSize - slot * (kButtonSize + kButtonGap);
    const uint32_t y = window.bounds.y + (kTitleBarHeight - kButtonSize) / 2;
    return {x, y, kButtonSize, kButtonSize};
}

WindowControl HitWindowControl(int32_t x, int32_t y, const Window& window) {
    if (PointInRect(x, y, ControlButtonRect(window, WindowControl::Close))) {
        return WindowControl::Close;
    }
    if (PointInRect(x, y, ControlButtonRect(window, WindowControl::Maximize))) {
        return WindowControl::Maximize;
    }
    if (PointInRect(x, y, ControlButtonRect(window, WindowControl::Minimize))) {
        return WindowControl::Minimize;
    }

    return WindowControl::None;
}

int32_t FindTopWindowAt(int32_t x, int32_t y, bool title_bar_only) {
    for (int32_t i = static_cast<int32_t>(g_Status.window_count) - 1; i >= 0; i--) {
        const Window& window = g_Windows[i];
        if (!window.visible || window.minimized) {
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

    Window selected;
    CopyWindow(selected, g_Windows[index]);
    for (uint32_t i = index; i + 1 < g_Status.window_count; i++) {
        CopyWindow(g_Windows[i], g_Windows[i + 1]);
    }
    CopyWindow(g_Windows[g_Status.window_count - 1], selected);
    g_DragWindowIndex = static_cast<int32_t>(g_Status.window_count - 1);
}

void MoveWindow(Window& window, int32_t x, int32_t y) {
    if (window.maximized) {
        return;
    }

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

    if (y < static_cast<int32_t>(kTopBarHeight + 6)) {
        y = static_cast<int32_t>(kTopBarHeight + 6);
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
    window.restore_bounds = bounds;
    window.color = color;
    window.visible = true;
    window.minimized = false;
    window.maximized = false;
    window.terminal = terminal;
    g_Status.window_count++;
    return true;
}

void MinimizeWindow(uint32_t index) {
    if (index >= g_Status.window_count) {
        return;
    }

    g_Windows[index].minimized = true;
    g_DragWindowIndex = -1;
}

void ToggleMaximizeWindow(uint32_t index) {
    if (index >= g_Status.window_count) {
        return;
    }

    Window& window = g_Windows[index];
    if (window.maximized) {
        window.bounds = window.restore_bounds;
        window.maximized = false;
        return;
    }

    window.restore_bounds = window.bounds;
    window.bounds = {
        8,
        kTopBarHeight + 8,
        g_Framebuffer.width > 16 ? g_Framebuffer.width - 16 : g_Framebuffer.width,
        g_Framebuffer.height > kTopBarHeight + kTaskBarHeight + 20
            ? g_Framebuffer.height - kTopBarHeight - kTaskBarHeight - 20
            : g_Framebuffer.height,
    };
    window.maximized = true;
}

void RestoreWindowFromTaskbar(uint32_t index) {
    if (index >= g_Status.window_count || !g_Windows[index].visible) {
        return;
    }

    g_Windows[index].minimized = false;
    BringWindowToFront(index);
}

bool InitWindowManager() {
    const uint32_t margin = 32;
    const uint32_t usable_width = g_Framebuffer.width > margin * 2 ? g_Framebuffer.width - margin * 2 : g_Framebuffer.width;
    const uint32_t usable_height = g_Framebuffer.height > kTopBarHeight + kTaskBarHeight + 48
        ? g_Framebuffer.height - kTopBarHeight - kTaskBarHeight - 48
        : g_Framebuffer.height;

    const Rect shell = {
        margin,
        kTopBarHeight + 30,
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
        AddWindow("Terminal", shell, kTheme.terminal, true) &&
        AddWindow("System Monitor", monitor, kTheme.monitor, false);
    return g_Status.window_manager_ready;
}

bool TerminalTextEquals(const char* a, const char* b) {
    if (!a || !b) {
        return false;
    }

    while (*a && *b) {
        if (*a != *b) {
            return false;
        }
        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

bool TerminalStartsWith(const char* text, const char* prefix) {
    if (!text || !prefix) {
        return false;
    }

    while (*prefix) {
        if (*text != *prefix) {
            return false;
        }
        text++;
        prefix++;
    }

    return true;
}

void TerminalAppendLine(const char* text, uint32_t color = kTheme.text) {
    if (!text) {
        return;
    }

    if (g_TerminalLineCount >= kMaxTerminalLines) {
        for (uint32_t i = 1; i < kMaxTerminalLines; i++) {
            g_TerminalLines[i - 1] = g_TerminalLines[i];
        }
        g_TerminalLineCount = kMaxTerminalLines - 1;
    }

    g_TerminalLines[g_TerminalLineCount].text = text;
    g_TerminalLines[g_TerminalLineCount].color = color;
    g_TerminalLineCount++;
}

void TerminalClear() {
    g_TerminalLineCount = 0;
    for (uint32_t i = 0; i < kMaxTerminalLines; i++) {
        g_TerminalLines[i].text = nullptr;
        g_TerminalLines[i].color = 0;
    }
}

bool ExecuteTerminalCommand(const char* command) {
    if (!command) {
        return false;
    }

    if (TerminalTextEquals(command, "help")) {
        TerminalAppendLine("help mem cpu clear version uptime", kTheme.accent);
        TerminalAppendLine("ls pwd cd cat reboot shutdown", kTheme.accent);
        return true;
    }
    if (TerminalTextEquals(command, "clear")) {
        TerminalClear();
        TerminalAppendLine("> clear", kTheme.text_muted);
        return true;
    }
    if (TerminalTextEquals(command, "mem")) {
        TerminalAppendLine("mem: heap online, pages tracked", kTheme.text);
        return true;
    }
    if (TerminalTextEquals(command, "cpu")) {
        TerminalAppendLine("cpu: x86_64 irq apic smp ready", kTheme.text);
        return true;
    }
    if (TerminalTextEquals(command, "version")) {
        TerminalAppendLine("version: Antigravity OS 0.13", kTheme.text);
        return true;
    }
    if (TerminalTextEquals(command, "uptime")) {
        TerminalAppendLine("uptime: boot session active", kTheme.text);
        return true;
    }
    if (TerminalTextEquals(command, "ls")) {
        TerminalAppendLine("boot kernel tmp readme", kTheme.text);
        return true;
    }
    if (TerminalTextEquals(command, "pwd")) {
        TerminalAppendLine(g_TerminalCwd, kTheme.text);
        return true;
    }
    if (TerminalTextEquals(command, "cd") || TerminalTextEquals(command, "cd /")) {
        g_TerminalCwd = "/";
        TerminalAppendLine("cwd: /", kTheme.text);
        return true;
    }
    if (TerminalTextEquals(command, "cd /boot")) {
        g_TerminalCwd = "/boot";
        TerminalAppendLine("cwd: /boot", kTheme.text);
        return true;
    }
    if (TerminalTextEquals(command, "cd /tmp")) {
        g_TerminalCwd = "/tmp";
        TerminalAppendLine("cwd: /tmp", kTheme.text);
        return true;
    }
    if (TerminalStartsWith(command, "cd ")) {
        TerminalAppendLine("cd: no such directory", kTheme.close);
        return true;
    }
    if (TerminalTextEquals(command, "cat readme") || TerminalTextEquals(command, "cat /tmp/readme")) {
        TerminalAppendLine("Antigravity OS terminal ready.", kTheme.text);
        return true;
    }
    if (TerminalStartsWith(command, "cat ")) {
        TerminalAppendLine("cat: file not found", kTheme.close);
        return true;
    }
    if (TerminalTextEquals(command, "reboot")) {
        g_TerminalRebootRequested = true;
        TerminalAppendLine("reboot: request queued", kTheme.minimize);
        return true;
    }
    if (TerminalTextEquals(command, "shutdown")) {
        g_TerminalShutdownRequested = true;
        TerminalAppendLine("shutdown: request queued", kTheme.minimize);
        return true;
    }

    TerminalAppendLine("shell: unknown command", kTheme.close);
    return false;
}

void SeedTerminalTranscript() {
    TerminalClear();
    g_TerminalRebootRequested = false;
    g_TerminalShutdownRequested = false;
    TerminalAppendLine("Kernel Console", kTheme.text);
    TerminalAppendLine("[INFO] Phase 13 terminal initialized", kTheme.accent);
    TerminalAppendLine("[INFO] Command parser ready", kTheme.accent);
    TerminalAppendLine("> help", kTheme.text_muted);
    ExecuteTerminalCommand("help");
    TerminalAppendLine("> mem", kTheme.text_muted);
    ExecuteTerminalCommand("mem");
    TerminalAppendLine("> cpu", kTheme.text_muted);
    ExecuteTerminalCommand("cpu");
    TerminalAppendLine("> version", kTheme.text_muted);
    ExecuteTerminalCommand("version");
    TerminalAppendLine("> uptime", kTheme.text_muted);
    ExecuteTerminalCommand("uptime");
}

bool RunTerminalCommandSelfTest() {
    TerminalClear();
    const bool help_ok = ExecuteTerminalCommand("help");
    const bool clear_ok = ExecuteTerminalCommand("clear") && g_TerminalLineCount == 1;
    const bool mem_ok = ExecuteTerminalCommand("mem");
    const bool cpu_ok = ExecuteTerminalCommand("cpu");
    const bool version_ok = ExecuteTerminalCommand("version");
    const bool uptime_ok = ExecuteTerminalCommand("uptime");
    const bool ls_ok = ExecuteTerminalCommand("ls");
    const bool pwd_ok = ExecuteTerminalCommand("pwd");
    const bool cd_ok = ExecuteTerminalCommand("cd /tmp") && TerminalTextEquals(g_TerminalCwd, "/tmp");
    const bool cat_ok = ExecuteTerminalCommand("cat readme");
    const bool reboot_ok = ExecuteTerminalCommand("reboot") && g_TerminalRebootRequested;
    const bool shutdown_ok = ExecuteTerminalCommand("shutdown") && g_TerminalShutdownRequested;

    return help_ok &&
        clear_ok &&
        mem_ok &&
        cpu_ok &&
        version_ok &&
        uptime_ok &&
        ls_ok &&
        pwd_ok &&
        cd_ok &&
        cat_ok &&
        reboot_ok &&
        shutdown_ok;
}

void DrawTerminalContents(const Window& window) {
    const uint32_t x = window.bounds.x + 18;
    uint32_t y = window.bounds.y + kTitleBarHeight + 18;

    for (uint32_t i = 0; i < g_TerminalLineCount; i++) {
        if (g_TerminalLines[i].text) {
            Graphics::DrawText(x, y, g_TerminalLines[i].text, g_TerminalLines[i].color);
        }
        y += 20;
        if (y + 16 >= window.bounds.y + window.bounds.height) {
            break;
        }
    }
}

void DrawSystemMonitorContents(const Window& window) {
    const uint32_t x = window.bounds.x + 18;
    uint32_t y = window.bounds.y + kTitleBarHeight + 18;

    Graphics::DrawText(x, y, "CPU 3%", kTheme.text);
    y += 24;
    Graphics::DrawText(x, y, "RAM 12 MB", kTheme.text);
    y += 24;
    Graphics::DrawText(x, y, "FPS 60", kTheme.text);
    y += 28;
    Graphics::FillRect({x, y, 150, 10}, 0xFF1B2632);
    Graphics::FillRect({x, y, 48, 10}, kTheme.accent);
}

void DrawWindowControl(const Window& window, WindowControl control, bool active) {
    const Rect rect = ControlButtonRect(window, control);
    const bool hovered = active && g_HoveredControl == control && g_HoveredWindowIndex >= 0;
    uint32_t color = hovered ? kTheme.button_hover : kTheme.button_idle;

    if (control == WindowControl::Close) {
        color = hovered ? kTheme.close : kTheme.button_idle;
    }

    Graphics::FillRect(rect, color);
    const uint32_t icon = control == WindowControl::Close
        ? kTheme.close
        : (control == WindowControl::Minimize ? kTheme.minimize : kTheme.maximize);

    if (control == WindowControl::Close) {
        Graphics::DrawLine(static_cast<int32_t>(rect.x + 5), static_cast<int32_t>(rect.y + 5),
            static_cast<int32_t>(rect.x + 11), static_cast<int32_t>(rect.y + 11), icon);
        Graphics::DrawLine(static_cast<int32_t>(rect.x + 11), static_cast<int32_t>(rect.y + 5),
            static_cast<int32_t>(rect.x + 5), static_cast<int32_t>(rect.y + 11), icon);
    } else if (control == WindowControl::Minimize) {
        Graphics::FillRect({rect.x + 4, rect.y + 11, 8, 2}, icon);
    } else if (window.maximized) {
        Graphics::DrawRect({rect.x + 4, rect.y + 5, 8, 7}, icon);
        Graphics::DrawRect({rect.x + 2, rect.y + 7, 8, 7}, icon);
    } else {
        Graphics::DrawRect({rect.x + 4, rect.y + 4, 8, 8}, icon);
    }
}

void DrawShadow(const Rect& bounds) {
    Graphics::FillRect({bounds.x + 14, bounds.y + 16, bounds.width, bounds.height}, kTheme.shadow_far);
    Graphics::FillRect({bounds.x + 9, bounds.y + 10, bounds.width, bounds.height}, kTheme.shadow_mid);
    Graphics::FillRect({bounds.x + 4, bounds.y + 5, bounds.width, bounds.height}, kTheme.shadow_near);
}

void ComposeWindow(const Window& window, bool active) {
    if (!window.visible || window.minimized) {
        return;
    }

    DrawShadow(window.bounds);
    Graphics::FillRect(window.bounds, window.color);
    Graphics::FillRect({window.bounds.x, window.bounds.y, window.bounds.width, kTitleBarHeight},
        active ? kTheme.title_active : kTheme.title_inactive);
    Graphics::FillRect({window.bounds.x, window.bounds.y + kTitleBarHeight - 1, window.bounds.width, 1},
        active ? kTheme.border_active : kTheme.border_inactive);
    Graphics::DrawText(window.bounds.x + 12, window.bounds.y + 8, window.title,
        active ? kTheme.text : kTheme.text_muted);

    DrawWindowControl(window, WindowControl::Minimize, active);
    DrawWindowControl(window, WindowControl::Maximize, active);
    DrawWindowControl(window, WindowControl::Close, active);

    for (uint32_t i = 0; i < kWindowBorder; i++) {
        Graphics::DrawRect({window.bounds.x + i, window.bounds.y + i, window.bounds.width - i * 2, window.bounds.height - i * 2},
            active ? kTheme.border_active : kTheme.border_inactive);
    }

    if (window.terminal) {
        DrawTerminalContents(window);
    } else {
        DrawSystemMonitorContents(window);
    }
}

void DrawWallpaper() {
    Graphics::FillRect({0, 0, g_Framebuffer.width, g_Framebuffer.height}, kTheme.desktop_bottom);
    const uint32_t band_height = g_Framebuffer.height / 4;
    Graphics::FillRect({0, 0, g_Framebuffer.width, band_height}, kTheme.desktop_top);

    for (uint32_t x = 0; x < g_Framebuffer.width; x += 48) {
        Graphics::DrawLine(static_cast<int32_t>(x), static_cast<int32_t>(kTopBarHeight),
            static_cast<int32_t>(x + 120), static_cast<int32_t>(g_Framebuffer.height - kTaskBarHeight), 0xFF1D2631);
    }
}

void DrawDesktopIcons() {
    const uint32_t x = 24;
    uint32_t y = kTopBarHeight + 28;
    Graphics::DrawImage({x, y, 28, 24}, 0xFF33485A);
    Graphics::FillRect({x + 4, y + 5, 20, 14}, kTheme.accent);
    Graphics::DrawText(x, y + 34, "Files", kTheme.text);

    y += 72;
    Graphics::DrawImage({x + 3, y, 22, 28}, 0xFF38404A);
    Graphics::FillRect({x + 7, y + 5, 14, 18}, kTheme.minimize);
    Graphics::DrawText(x, y + 38, "Disk", kTheme.text);
}

void DrawBars() {
    Graphics::FillRect({0, 0, g_Framebuffer.width, kTopBarHeight}, kTheme.panel);
    Graphics::FillRect({0, kTopBarHeight - 1, g_Framebuffer.width, 1}, kTheme.panel_edge);
    Graphics::FillRect({0, g_Framebuffer.height - kTaskBarHeight, g_Framebuffer.width, kTaskBarHeight}, kTheme.panel);
    Graphics::FillRect({0, g_Framebuffer.height - kTaskBarHeight, g_Framebuffer.width, 1}, kTheme.panel_edge);

    Graphics::FillRect({14, 12, 14, 14}, kTheme.accent);
    Graphics::DrawText(38, 12, "Antigravity OS", kTheme.text);
    Graphics::DrawText(190, 12, "CPU 3%", kTheme.text_muted);
    Graphics::DrawText(282, 12, "RAM 12 MB", kTheme.text_muted);
    Graphics::DrawText(410, 12, "FPS 60", kTheme.text_muted);
    if (g_Framebuffer.width > 92) {
        Graphics::DrawText(g_Framebuffer.width - 82, 12, "22:45", kTheme.text);
    }
}

void DrawTaskbar() {
    uint32_t x = 18;
    const uint32_t y = g_Framebuffer.height - kTaskBarHeight + 8;

    for (uint32_t i = 0; i < g_Status.window_count; i++) {
        const Window& window = g_Windows[i];
        if (!window.visible) {
            continue;
        }

        const uint32_t button_width = window.terminal ? 112 : 156;
        Graphics::FillRect({x, y, button_width, 26}, window.minimized ? 0xFF1C242E : 0xFF2D3945);
        Graphics::DrawRect({x, y, button_width, 26}, window.minimized ? kTheme.border_inactive : kTheme.border_active);
        Graphics::DrawText(x + 10, y + 7, window.title, window.minimized ? kTheme.text_muted : kTheme.text);
        x += button_width + 10;
    }
}

bool ComposeDesktop() {
    g_CursorBackup.valid = false;
    if (g_BackBufferReady) {
        BeginDrawToBackBuffer(ScreenRect());
    } else {
        BeginDrawToFramebuffer();
    }

    DrawWallpaper();
    DrawDesktopIcons();
    DrawBars();

    int32_t active_window = -1;
    for (int32_t i = static_cast<int32_t>(g_Status.window_count) - 1; i >= 0; i--) {
        if (g_Windows[i].visible && !g_Windows[i].minimized) {
            active_window = i;
            break;
        }
    }

    for (uint32_t i = 0; i < g_Status.window_count; i++) {
        ComposeWindow(g_Windows[i], static_cast<int32_t>(i) == active_window);
    }

    DrawTaskbar();
    BeginDrawToFramebuffer();
    if (g_BackBufferReady) {
        CopyBackBufferToFramebuffer(ScreenRect());
    }
    PaintCursor();
    return true;
}

void RedrawDirtyRegion(const Rect& dirty) {
    if (!g_BackBufferReady) {
        ComposeDesktop();
        return;
    }

    const Rect clipped = IntersectRect(dirty, ScreenRect());
    if (RectEmpty(clipped)) {
        PaintCursor();
        return;
    }

    BeginDrawToBackBuffer(clipped);
    DrawWallpaper();
    DrawDesktopIcons();
    DrawBars();

    int32_t active_window = -1;
    for (int32_t i = static_cast<int32_t>(g_Status.window_count) - 1; i >= 0; i--) {
        if (g_Windows[i].visible && !g_Windows[i].minimized) {
            active_window = i;
            break;
        }
    }

    for (uint32_t i = 0; i < g_Status.window_count; i++) {
        ComposeWindow(g_Windows[i], static_cast<int32_t>(i) == active_window);
    }

    DrawTaskbar();
    BeginDrawToFramebuffer();
    CopyBackBufferToFramebuffer(clipped);
    PaintCursor();
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

int32_t FindTaskbarWindowAt(int32_t x, int32_t y) {
    if (y < static_cast<int32_t>(g_Framebuffer.height - kTaskBarHeight)) {
        return -1;
    }

    int32_t button_x = 18;
    const int32_t button_y = static_cast<int32_t>(g_Framebuffer.height - kTaskBarHeight + 8);
    for (uint32_t i = 0; i < g_Status.window_count; i++) {
        const Window& window = g_Windows[i];
        if (!window.visible) {
            continue;
        }

        const int32_t button_width = window.terminal ? 112 : 156;
        if (PointInRect(x, y, {
            static_cast<uint32_t>(button_x),
            static_cast<uint32_t>(button_y),
            static_cast<uint32_t>(button_width),
            26,
        })) {
            return static_cast<int32_t>(i);
        }
        button_x += button_width + 10;
    }

    return -1;
}

void UpdateHoverState() {
    g_HoveredWindowIndex = FindTopWindowAt(g_MouseX, g_MouseY, false);
    g_HoveredControl = WindowControl::None;
    g_CursorKind = CursorKind::Arrow;

    if (FindTaskbarWindowAt(g_MouseX, g_MouseY) >= 0) {
        g_CursorKind = CursorKind::Hand;
        return;
    }

    if (g_HoveredWindowIndex < 0) {
        return;
    }

    const Window& window = g_Windows[g_HoveredWindowIndex];
    g_HoveredControl = HitWindowControl(g_MouseX, g_MouseY, window);
    if (g_HoveredControl != WindowControl::None) {
        g_CursorKind = CursorKind::Hand;
    } else if (PointInTitleBar(g_MouseX, g_MouseY, window)) {
        g_CursorKind = CursorKind::Move;
    }
}

void HandleMouseClick(int32_t x, int32_t y, uint32_t button) {
    if (button != 0) {
        return;
    }

    const int32_t taskbar_index = FindTaskbarWindowAt(x, y);
    if (taskbar_index >= 0) {
        RestoreWindowFromTaskbar(static_cast<uint32_t>(taskbar_index));
        return;
    }

    const int32_t window_index = FindTopWindowAt(x, y, false);
    if (window_index < 0) {
        return;
    }

    Window& window = g_Windows[window_index];
    const WindowControl control = HitWindowControl(x, y, window);
    if (control == WindowControl::Close) {
        CloseWindow(static_cast<uint32_t>(window_index));
    } else if (control == WindowControl::Minimize) {
        MinimizeWindow(static_cast<uint32_t>(window_index));
    } else if (control == WindowControl::Maximize) {
        ToggleMaximizeWindow(static_cast<uint32_t>(window_index));
    }
}

void DispatchEvent(const GuiEvent& event) {
    switch (event.type) {
        case GuiEventType::MouseMove:
            if (g_LeftButtonDown && g_DragWindowIndex >= 0) {
                const Rect old_window_rect = WindowVisualRect(g_Windows[g_DragWindowIndex]);
                const Rect old_cursor_rect = CursorRectAt(g_MouseX, g_MouseY);
                RestoreCursorBackground();
                g_MouseX = event.x;
                g_MouseY = event.y;
                ClampMouseToScreen();
                g_Dragging = true;
                MoveWindow(g_Windows[g_DragWindowIndex], g_MouseX - g_DragOffsetX, g_MouseY - g_DragOffsetY);
                const Rect new_window_rect = WindowVisualRect(g_Windows[g_DragWindowIndex]);
                const Rect new_cursor_rect = CursorRectAt(g_MouseX, g_MouseY);
                UpdateHoverState();
                RedrawDirtyRegion(UnionRect(UnionRect(old_window_rect, new_window_rect), UnionRect(old_cursor_rect, new_cursor_rect)));
            } else {
                MoveCursorOnly(event.x, event.y);
            }
            break;

        case GuiEventType::MouseDown:
            RestoreCursorBackground();
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
                    if (HitWindowControl(g_MouseX, g_MouseY, window) != WindowControl::None || window.maximized) {
                        g_DragWindowIndex = -1;
                    } else {
                        g_DragOffsetX = g_MouseX - static_cast<int32_t>(window.bounds.x);
                        g_DragOffsetY = g_MouseY - static_cast<int32_t>(window.bounds.y);
                    }
                }
            }
            UpdateHoverState();
            ComposeDesktop();
            break;

        case GuiEventType::MouseUp:
            RestoreCursorBackground();
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
            UpdateHoverState();
            ComposeDesktop();
            break;

        case GuiEventType::Click:
            RestoreCursorBackground();
            g_MouseX = event.x;
            g_MouseY = event.y;
            ClampMouseToScreen();
            HandleMouseClick(g_MouseX, g_MouseY, event.button);
            UpdateHoverState();
            ComposeDesktop();
            break;

        case GuiEventType::Drag:
            if (g_LeftButtonDown && g_DragWindowIndex >= 0) {
                const Rect old_window_rect = WindowVisualRect(g_Windows[g_DragWindowIndex]);
                const Rect old_cursor_rect = CursorRectAt(g_MouseX, g_MouseY);
                RestoreCursorBackground();
                g_MouseX = event.x;
                g_MouseY = event.y;
                ClampMouseToScreen();
                g_Dragging = true;
                MoveWindow(g_Windows[g_DragWindowIndex], g_MouseX - g_DragOffsetX, g_MouseY - g_DragOffsetY);
                const Rect new_window_rect = WindowVisualRect(g_Windows[g_DragWindowIndex]);
                const Rect new_cursor_rect = CursorRectAt(g_MouseX, g_MouseY);
                UpdateHoverState();
                RedrawDirtyRegion(UnionRect(UnionRect(old_window_rect, new_window_rect), UnionRect(old_cursor_rect, new_cursor_rect)));
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
    g_BackBufferReady =
        g_Framebuffer.width <= kBackBufferMaxWidth &&
        g_Framebuffer.height <= kBackBufferMaxHeight;
    g_MouseX = static_cast<int32_t>(g_Framebuffer.width / 2);
    g_MouseY = static_cast<int32_t>(g_Framebuffer.height / 2);
    g_Status.event_queue_ready = true;
    g_Status.mouse_ready = true;

    if (!InitWindowManager()) {
        return false;
    }

    if (!RunTerminalCommandSelfTest()) {
        KernelLog(LogLevel::Warn, "Terminal command self-test failed");
        return false;
    }

    SeedTerminalTranscript();
    UpdateHoverState();
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
