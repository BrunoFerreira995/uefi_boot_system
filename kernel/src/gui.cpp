#include "gui.hpp"

#include "kernel.hpp"
#include "userspace.hpp"

namespace {

struct Rect {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
};

enum class AppKind : uint8_t {
    FileManager,
    TextEditor,
    ImageViewer,
    Calculator,
    Settings,
    TaskManager,
    PackageManager,
    SystemMonitor,
    TerminalEmulator,
    SoftwareCenter,
};

struct Window {
    const char* title;
    Rect bounds;
    Rect restore_bounds;
    uint32_t color;
    bool visible;
    bool minimized;
    bool maximized;
    bool focused;
    AppKind app;
    uint8_t desktop;
    uint64_t process_id;
    uint64_t thread_id;
};

struct AppPlacementState {
    AppKind app;
    Rect last_bounds;
    bool valid;
};

enum class AppLifecycleState : uint8_t {
    NotRunning,
    Opening,
    Running,
    Failed,
    NotResponding,
};

struct AppRuntimeState {
    AppKind app;
    AppLifecycleState state;
    uint64_t process_id;
    uint64_t thread_id;
    uint32_t launch_count;
    uint32_t missed_heartbeat_count;
    bool valid;
};

static constexpr uint32_t kMaxWindows = 12;
static constexpr uint32_t kEventQueueCapacity = 64;
static constexpr uint32_t kTopBarHeight = 38;
static constexpr uint32_t kTaskBarHeight = 78;
static constexpr uint32_t kTitleBarHeight = 30;
static constexpr uint32_t kWindowBorder = 2;
static constexpr uint32_t kButtonSize = 16;
static constexpr uint32_t kButtonGap = 7;
static constexpr uint32_t kCursorHeight = 16;
static constexpr uint32_t kCursorBackupSize = 20;
static constexpr uint32_t kBackBufferMaxWidth = 2048;
static constexpr uint32_t kBackBufferMaxHeight = 2048;
static constexpr uint32_t kBackBufferPixels = kBackBufferMaxWidth * kBackBufferMaxHeight;
static constexpr uint8_t kVirtualDesktopCount = 4;
static constexpr uint32_t kFontCacheEntries = 32;
static constexpr uint32_t kDefaultAppMinWidth = 480;
static constexpr uint32_t kDefaultAppMinHeight = 320;
static constexpr uint32_t kCompactAppMinWidth = 360;
static constexpr uint32_t kCompactAppMinHeight = 260;
static constexpr uint32_t kApplicationCapacity = 10;
static constexpr uint32_t kLauncherButtonWidth = 76;
static constexpr uint32_t kLauncherButtonHeight = 26;

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
    char text[96];
    uint32_t color;
};

static constexpr uint32_t kMaxTerminalLines = 64;
static constexpr uint32_t kVisibleTerminalLines = 12;
static constexpr uint32_t kTerminalHistoryEntries = 8;
static constexpr uint32_t kTerminalCommandLength = 64;

GuiStatus g_Status {};
FramebufferInfo g_Framebuffer {};
Window g_Windows[kMaxWindows];
AppPlacementState g_AppPlacements[kMaxWindows];
AppRuntimeState g_AppRuntimes[kApplicationCapacity];
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
uint8_t g_ActiveDesktop = 0;
int32_t g_ActiveWindowIndex = -1;
bool g_DebugFullRedraw = true;
int32_t g_TerminalScrollOffset = 0;
WindowControl g_HoveredControl = WindowControl::None;
int32_t g_HoveredWindowIndex = -1;
CursorKind g_CursorKind = CursorKind::Arrow;
CursorBackup g_CursorBackup {};
TerminalLine g_TerminalLines[kMaxTerminalLines];
uint32_t g_TerminalLineCount = 0;
const char* g_TerminalCwd = "/";
bool g_TerminalRebootRequested = false;
bool g_TerminalShutdownRequested = false;
char g_TerminalHistory[kTerminalHistoryEntries][kTerminalCommandLength];
uint32_t g_TerminalHistoryCount = 0;
int32_t g_TerminalHistoryCursor = -1;
const char* g_NotificationText = nullptr;
uint32_t g_NotificationColor = kTheme.text_muted;

bool PointInRect(int32_t x, int32_t y, const Rect& rect);
void CloseWindow(uint32_t index);
void RedrawDirtyRegion(const Rect& dirty);
uint32_t AppMinWidth(AppKind app);
uint32_t AppMinHeight(AppKind app);
Rect UsableDesktopRect();
Rect ClampWindowToUsableArea(Rect bounds, AppKind app);
void RememberAppPlacement(AppKind app, const Rect& bounds);

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
    g_ActiveDesktop = 0;
    g_ActiveWindowIndex = -1;
    g_DebugFullRedraw = true;
    g_TerminalScrollOffset = 0;

    for (uint32_t i = 0; i < kMaxWindows; i++) {
        g_Windows[i].title = nullptr;
        g_Windows[i].bounds = {0, 0, 0, 0};
        g_Windows[i].restore_bounds = {0, 0, 0, 0};
        g_Windows[i].color = 0;
        g_Windows[i].visible = false;
        g_Windows[i].minimized = false;
        g_Windows[i].maximized = false;
        g_Windows[i].focused = false;
        g_Windows[i].app = AppKind::FileManager;
        g_Windows[i].desktop = 0;
        g_Windows[i].process_id = 0;
        g_Windows[i].thread_id = 0;
    }

    for (uint32_t i = 0; i < kMaxWindows; i++) {
        g_AppPlacements[i].app = AppKind::FileManager;
        g_AppPlacements[i].last_bounds = {0, 0, 0, 0};
        g_AppPlacements[i].valid = false;
    }

    for (uint32_t i = 0; i < kApplicationCapacity; i++) {
        g_AppRuntimes[i].app = AppKind::FileManager;
        g_AppRuntimes[i].state = AppLifecycleState::NotRunning;
        g_AppRuntimes[i].process_id = 0;
        g_AppRuntimes[i].thread_id = 0;
        g_AppRuntimes[i].launch_count = 0;
        g_AppRuntimes[i].missed_heartbeat_count = 0;
        g_AppRuntimes[i].valid = false;
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
        g_TerminalLines[i].text[0] = '\0';
        g_TerminalLines[i].color = 0;
    }
    for (uint32_t i = 0; i < kTerminalHistoryEntries; i++) {
        g_TerminalHistory[i][0] = '\0';
    }
    g_TerminalHistoryCount = 0;
    g_TerminalHistoryCursor = -1;
    g_NotificationText = nullptr;
    g_NotificationColor = kTheme.text_muted;
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
    destination.focused = source.focused;
    destination.app = source.app;
    destination.desktop = source.desktop;
    destination.process_id = source.process_id;
    destination.thread_id = source.thread_id;
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
        if (!window.visible || window.minimized || window.desktop != g_ActiveDesktop) {
            continue;
        }

        if (title_bar_only ? PointInTitleBar(x, y, window) : PointInRect(x, y, window.bounds)) {
            return i;
        }
    }

    return -1;
}

int32_t BringWindowToFront(uint32_t index) {
    if (index >= g_Status.window_count || index + 1 == g_Status.window_count) {
        return index < g_Status.window_count ? static_cast<int32_t>(index) : -1;
    }

    Window selected;
    CopyWindow(selected, g_Windows[index]);
    for (uint32_t i = index; i + 1 < g_Status.window_count; i++) {
        CopyWindow(g_Windows[i], g_Windows[i + 1]);
    }
    CopyWindow(g_Windows[g_Status.window_count - 1], selected);
    g_DragWindowIndex = static_cast<int32_t>(g_Status.window_count - 1);
    return g_DragWindowIndex;
}

int32_t FindTopVisibleWindow() {
    for (int32_t i = static_cast<int32_t>(g_Status.window_count) - 1; i >= 0; i--) {
        const Window& window = g_Windows[i];
        if (window.visible && !window.minimized && window.desktop == g_ActiveDesktop) {
            return i;
        }
    }
    return -1;
}

void ClearWindowFocus() {
    for (uint32_t i = 0; i < g_Status.window_count; i++) {
        g_Windows[i].focused = false;
    }
    g_ActiveWindowIndex = -1;
}

bool FocusWindow(uint32_t index) {
    if (index >= g_Status.window_count || !g_Windows[index].visible ||
        g_Windows[index].minimized || g_Windows[index].desktop != g_ActiveDesktop) {
        return false;
    }

    ClearWindowFocus();
    const int32_t front_index = BringWindowToFront(index);
    if (front_index < 0) {
        return false;
    }

    g_ActiveWindowIndex = front_index;
    g_Windows[front_index].focused = true;
    KernelLog(LogLevel::Info, "[GUI] focus changed");
    return true;
}

void FocusTopVisibleWindow() {
    const int32_t top = FindTopVisibleWindow();
    if (top >= 0) {
        FocusWindow(static_cast<uint32_t>(top));
    } else {
        ClearWindowFocus();
    }
}

void MoveWindow(Window& window, int32_t x, int32_t y) {
    if (window.maximized) {
        return;
    }

    Rect moved = window.bounds;
    moved.x = x < 0 ? 0 : static_cast<uint32_t>(x);
    moved.y = y < 0 ? 0 : static_cast<uint32_t>(y);
    window.bounds = ClampWindowToUsableArea(moved, window.app);
    RememberAppPlacement(window.app, window.bounds);
}

bool AddWindow(const char* title,
               Rect bounds,
               uint32_t color,
               AppKind app,
               uint8_t desktop,
               uint64_t process_id = 0,
               uint64_t thread_id = 0) {
    if (!title || bounds.width == 0 || bounds.height == 0 ||
        desktop >= kVirtualDesktopCount || g_Status.window_count >= kMaxWindows) {
        return false;
    }

    Window& window = g_Windows[g_Status.window_count];
    window.title = title;
    window.bounds = ClampWindowToUsableArea(bounds, app);
    window.restore_bounds = window.bounds;
    window.color = color;
    window.visible = true;
    window.minimized = false;
    window.maximized = false;
    window.focused = false;
    window.app = app;
    window.desktop = desktop;
    window.process_id = process_id;
    window.thread_id = thread_id;
    RememberAppPlacement(app, window.bounds);
    g_Status.window_count++;
    KernelLog(LogLevel::Info, "[GUI] window registered");
    return true;
}

bool ResizeWindow(Window& window, uint32_t width, uint32_t height) {
    if (window.maximized) return false;
    Rect resized = window.bounds;
    resized.width = width;
    resized.height = height;
    window.bounds = ClampWindowToUsableArea(resized, window.app);
    RememberAppPlacement(window.app, window.bounds);
    return window.bounds.width >= AppMinWidth(window.app) &&
        window.bounds.height >= AppMinHeight(window.app);
}

enum class SnapEdge : uint8_t { Left, Right, Top };
bool SnapWindow(Window& window, SnapEdge edge) {
    if (g_Framebuffer.width == 0 || g_Framebuffer.height <= kTopBarHeight + kTaskBarHeight) return false;
    window.restore_bounds = window.bounds;
    const Rect usable = UsableDesktopRect();
    if (edge == SnapEdge::Top) {
        window.bounds = usable;
        window.maximized = true;
    } else {
        const uint32_t half = usable.width / 2;
        window.bounds = {edge == SnapEdge::Left ? usable.x : usable.x + half, usable.y,
            edge == SnapEdge::Left ? half : usable.width - half, usable.height};
        window.maximized = false;
    }
    window.bounds = ClampWindowToUsableArea(window.bounds, window.app);
    RememberAppPlacement(window.app, window.bounds);
    return true;
}

bool SwitchVirtualDesktop(uint8_t desktop) {
    if (desktop >= kVirtualDesktopCount) return false;
    g_ActiveDesktop = desktop;
    g_DragWindowIndex = -1;
    FocusTopVisibleWindow();
    return true;
}

void MinimizeWindow(uint32_t index) {
    if (index >= g_Status.window_count) {
        return;
    }

    g_Windows[index].minimized = true;
    g_DragWindowIndex = -1;
    if (g_ActiveWindowIndex == static_cast<int32_t>(index)) {
        FocusTopVisibleWindow();
    }
}

void ToggleMaximizeWindow(uint32_t index) {
    if (index >= g_Status.window_count) {
        return;
    }

    Window& window = g_Windows[index];
    if (window.maximized) {
        window.bounds = ClampWindowToUsableArea(window.restore_bounds, window.app);
        window.maximized = false;
        RememberAppPlacement(window.app, window.bounds);
        return;
    }

    window.restore_bounds = window.bounds;
    const Rect usable = UsableDesktopRect();
    window.bounds = {
        usable.x + 8,
        usable.y,
        usable.width > 16 ? usable.width - 16 : usable.width,
        usable.height,
    };
    window.bounds = ClampWindowToUsableArea(window.bounds, window.app);
    window.maximized = true;
}

void RestoreWindowFromTaskbar(uint32_t index) {
    if (index >= g_Status.window_count || !g_Windows[index].visible) {
        return;
    }

    g_Windows[index].minimized = false;
    FocusWindow(index);
}

struct ApplicationDescriptor {
    AppKind app;
    const char* id;
    const char* name;
    const char* executable_path;
    const char* icon_path;
    uint32_t color;
    uint8_t desktop;
    uint32_t min_width;
    uint32_t min_height;
    uint32_t preferred_width;
    uint32_t preferred_height;
    bool compact;
};

static constexpr ApplicationDescriptor kApplications[] = {
    {AppKind::FileManager, "files", "File Manager", "/system/apps/files.app", "/system/icons/files.bmp", kTheme.window, 0, 480, 320, 640, 420, false},
    {AppKind::TextEditor, "edit", "Text Editor", "/system/apps/editor.app", "/system/icons/editor.bmp", 0xFF293844, 1, 560, 360, 760, 520, false},
    {AppKind::ImageViewer, "image", "Image Viewer", "/system/apps/images.app", "/system/icons/images.bmp", 0xFF263C3A, 1, 480, 320, 680, 440, false},
    {AppKind::Calculator, "calc", "Calculator", "/system/apps/calculator.app", "/system/icons/calculator.bmp", 0xFF3C3443, 1, 360, 260, 380, 300, true},
    {AppKind::Settings, "set", "Settings", "/system/apps/settings.app", "/system/icons/settings.bmp", 0xFF303A45, 2, 420, 300, 520, 360, true},
    {AppKind::TaskManager, "tasks", "Task Manager", "/system/apps/tasks.app", "/system/icons/tasks.bmp", 0xFF303F38, 2, 480, 320, 640, 420, false},
    {AppKind::PackageManager, "pkg", "Package Manager", "/system/apps/packages.app", "/system/icons/packages.bmp", 0xFF3C3847, 2, 480, 320, 640, 420, false},
    {AppKind::SystemMonitor, "mon", "System Monitor", "/system/apps/monitor.app", "/system/icons/monitor.bmp", kTheme.monitor, 0, 420, 300, 520, 360, true},
    {AppKind::TerminalEmulator, "term", "Terminal Emulator", "/bin/terminal", "/system/icons/terminal.bmp", kTheme.terminal, 0, 560, 360, 760, 480, false},
    {AppKind::SoftwareCenter, "store", "Software Center", "/system/apps/software.app", "/system/icons/software.bmp", 0xFF343D3B, 3, 480, 320, 680, 440, false},
};

static constexpr uint32_t kApplicationCount = sizeof(kApplications) / sizeof(kApplications[0]);

bool ApplicationRegistered(AppKind app) {
    for (uint32_t i = 0; i < kApplicationCount; i++) {
        if (kApplications[i].app == app) {
            return true;
        }
    }
    return false;
}

const ApplicationDescriptor* FindApplication(AppKind app) {
    for (uint32_t i = 0; i < kApplicationCount; i++) {
        if (kApplications[i].app == app) {
            return &kApplications[i];
        }
    }
    return nullptr;
}

const ApplicationDescriptor* FindApplicationById(const char* id) {
    if (!id) {
        return nullptr;
    }

    for (uint32_t i = 0; i < kApplicationCount; i++) {
        const char* candidate = kApplications[i].id;
        const char* requested = id;
        bool matches = true;
        while (*candidate || *requested) {
            if (*candidate != *requested) {
                matches = false;
                break;
            }
            candidate++;
            requested++;
        }
        if (matches) {
            return &kApplications[i];
        }
    }

    return nullptr;
}

AppRuntimeState* FindAppRuntime(AppKind app) {
    for (uint32_t i = 0; i < kApplicationCapacity; i++) {
        if (g_AppRuntimes[i].valid && g_AppRuntimes[i].app == app) {
            return &g_AppRuntimes[i];
        }
    }
    return nullptr;
}

bool InitApplicationRegistry() {
    if (kApplicationCount > kApplicationCapacity) {
        return false;
    }

    for (uint32_t i = 0; i < kApplicationCount; i++) {
        g_AppRuntimes[i].app = kApplications[i].app;
        g_AppRuntimes[i].state = AppLifecycleState::NotRunning;
        g_AppRuntimes[i].process_id = 0;
        g_AppRuntimes[i].thread_id = 0;
        g_AppRuntimes[i].launch_count = 0;
        g_AppRuntimes[i].missed_heartbeat_count = 0;
        g_AppRuntimes[i].valid = true;
    }

    return true;
}

void ShowNotification(const char* text, uint32_t color) {
    g_NotificationText = text;
    g_NotificationColor = color;
}

Rect UsableDesktopRect() {
    const uint32_t width = g_Framebuffer.width;
    const uint32_t top = kTopBarHeight + 8;
    const uint32_t bottom = g_Framebuffer.height > kTaskBarHeight + 8
        ? g_Framebuffer.height - kTaskBarHeight - 8
        : g_Framebuffer.height;
    const uint32_t height = bottom > top ? bottom - top : g_Framebuffer.height;
    return {0, top, width, height};
}

uint32_t AppMinWidth(AppKind app) {
    const ApplicationDescriptor* descriptor = FindApplication(app);
    if (!descriptor) {
        return kDefaultAppMinWidth;
    }
    return descriptor->min_width;
}

uint32_t AppMinHeight(AppKind app) {
    const ApplicationDescriptor* descriptor = FindApplication(app);
    if (!descriptor) {
        return kDefaultAppMinHeight;
    }
    return descriptor->min_height;
}

Rect ClampWindowToUsableArea(Rect bounds, AppKind app) {
    const Rect usable = UsableDesktopRect();
    const uint32_t min_width = AppMinWidth(app);
    const uint32_t min_height = AppMinHeight(app);

    if (bounds.width < min_width && usable.width >= min_width) {
        bounds.width = min_width;
    }
    if (bounds.height < min_height && usable.height >= min_height) {
        bounds.height = min_height;
    }
    if (bounds.width > usable.width) {
        bounds.width = usable.width;
    }
    if (bounds.height > usable.height) {
        bounds.height = usable.height;
    }

    const uint32_t max_x = usable.width > bounds.width ? usable.x + usable.width - bounds.width : usable.x;
    const uint32_t max_y = usable.height > bounds.height ? usable.y + usable.height - bounds.height : usable.y;

    if (bounds.x < usable.x) {
        bounds.x = usable.x;
    } else if (bounds.x > max_x) {
        bounds.x = max_x;
    }

    if (bounds.y < usable.y) {
        bounds.y = usable.y;
    } else if (bounds.y > max_y) {
        bounds.y = max_y;
    }

    return bounds;
}

Rect CenterWindowInUsableArea(uint32_t width, uint32_t height, AppKind app) {
    const Rect usable = UsableDesktopRect();
    Rect bounds = {
        usable.width > width ? usable.x + (usable.width - width) / 2 : usable.x,
        usable.height > height ? usable.y + (usable.height - height) / 2 : usable.y,
        width,
        height,
    };
    return ClampWindowToUsableArea(bounds, app);
}

AppPlacementState* FindAppPlacement(AppKind app) {
    for (uint32_t i = 0; i < kMaxWindows; i++) {
        if (g_AppPlacements[i].valid && g_AppPlacements[i].app == app) {
            return &g_AppPlacements[i];
        }
    }
    return nullptr;
}

void RememberAppPlacement(AppKind app, const Rect& bounds) {
    AppPlacementState* existing = FindAppPlacement(app);
    if (existing) {
        existing->last_bounds = bounds;
        return;
    }

    for (uint32_t i = 0; i < kMaxWindows; i++) {
        if (!g_AppPlacements[i].valid) {
            g_AppPlacements[i].app = app;
            g_AppPlacements[i].last_bounds = bounds;
            g_AppPlacements[i].valid = true;
            return;
        }
    }
}

Rect InitialWindowBounds(const ApplicationDescriptor& app, uint32_t ordinal) {
    AppPlacementState* placement = FindAppPlacement(app.app);
    if (placement) {
        return ClampWindowToUsableArea(placement->last_bounds, app.app);
    }

    Rect centered = CenterWindowInUsableArea(app.preferred_width, app.preferred_height, app.app);
    const uint32_t stagger = (ordinal % 3) * 28;
    centered.x += stagger;
    centered.y += stagger;
    return ClampWindowToUsableArea(centered, app.app);
}

int32_t FindWindowByApp(AppKind app, uint8_t desktop) {
    for (uint32_t i = 0; i < g_Status.window_count; i++) {
        if (g_Windows[i].app == app && g_Windows[i].desktop == desktop) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

bool RestoreOrFocusRunningApp(const ApplicationDescriptor& app) {
    const int32_t existing = FindWindowByApp(app.app, app.desktop);
    if (existing < 0) {
        return false;
    }

    g_Windows[existing].visible = true;
    g_Windows[existing].minimized = false;
    g_Windows[existing].maximized = false;
    if (g_ActiveDesktop != app.desktop) {
        SwitchVirtualDesktop(app.desktop);
    }
    FocusWindow(static_cast<uint32_t>(existing));
    ShowNotification("app restored", kTheme.maximize);
    return true;
}

bool LaunchApplication(const ApplicationDescriptor& app) {
    AppRuntimeState* runtime = FindAppRuntime(app.app);
    if (!runtime) {
        ShowNotification("missing app id", kTheme.close);
        KernelLog(LogLevel::Warn, "[APP] missing app id");
        return false;
    }

    if (runtime->state == AppLifecycleState::Running ||
        runtime->state == AppLifecycleState::NotResponding) {
        return RestoreOrFocusRunningApp(app);
    }

    runtime->state = AppLifecycleState::Opening;
    uint64_t process_id = 0;
    uint64_t thread_id = 0;
    if (!KernelLaunchUserApplication(app.id, app.executable_path, process_id, thread_id)) {
        runtime->state = AppLifecycleState::Failed;
        ShowNotification("app launch failed", kTheme.close);
        return false;
    }

    const int32_t hidden = FindWindowByApp(app.app, app.desktop);
    if (hidden >= 0) {
        Window& window = g_Windows[hidden];
        window.title = app.name;
        window.bounds = InitialWindowBounds(app, runtime->launch_count);
        window.restore_bounds = window.bounds;
        window.color = app.color;
        window.visible = true;
        window.minimized = false;
        window.maximized = false;
        window.process_id = process_id;
        window.thread_id = thread_id;
    } else {
        const Rect bounds = InitialWindowBounds(app, runtime->launch_count);
        if (!AddWindow(app.name, bounds, app.color, app.app, app.desktop, process_id, thread_id)) {
            runtime->state = AppLifecycleState::Failed;
            ShowNotification("app launch failed", kTheme.close);
            return false;
        }
    }

    if (!KernelStartUserApplicationEventLoop(process_id, app.id, thread_id)) {
        runtime->state = AppLifecycleState::Failed;
        ShowNotification("app launch failed", kTheme.close);
        return false;
    }

    const int32_t window_index = FindWindowByApp(app.app, app.desktop);
    if (window_index >= 0) {
        g_Windows[window_index].thread_id = thread_id;
    }

    runtime->state = AppLifecycleState::Running;
    runtime->process_id = process_id;
    runtime->thread_id = thread_id;
    runtime->launch_count++;
    runtime->missed_heartbeat_count = 0;
    if (g_ActiveDesktop != app.desktop) {
        SwitchVirtualDesktop(app.desktop);
    }
    RestoreOrFocusRunningApp(app);
    ShowNotification("app running", kTheme.maximize);
    return true;
}

bool LaunchApplicationById(const char* app_id) {
    const ApplicationDescriptor* app = FindApplicationById(app_id);
    if (!app) {
        ShowNotification("missing app id", kTheme.close);
        KernelLog(LogLevel::Warn, "[APP] missing app id");
        return false;
    }

    return LaunchApplication(*app);
}

void AppWatchdogTick() {
    for (uint32_t i = 0; i < kApplicationCapacity; i++) {
        AppRuntimeState& runtime = g_AppRuntimes[i];
        if (!runtime.valid || runtime.state != AppLifecycleState::Running) {
            continue;
        }

        runtime.missed_heartbeat_count++;
        if (runtime.missed_heartbeat_count >= 3) {
            runtime.state = AppLifecycleState::NotResponding;
            ShowNotification("app not responding", kTheme.minimize);
            KernelLog(LogLevel::Warn, "[APP] watchdog unresponsive");
        }
    }
}

void AppHeartbeat(AppKind app) {
    AppRuntimeState* runtime = FindAppRuntime(app);
    if (!runtime) {
        return;
    }

    runtime->missed_heartbeat_count = 0;
    if (runtime->state == AppLifecycleState::NotResponding) {
        runtime->state = AppLifecycleState::Running;
    }
}

bool InitWindowManager() {
    const bool registry_ready = InitApplicationRegistry();
    const bool default_apps_launched =
        LaunchApplicationById("files") &&
        LaunchApplicationById("term");

    g_Status.window_manager_ready = registry_ready && default_apps_launched;
    FocusTopVisibleWindow();
    return g_Status.window_manager_ready;
}

uint32_t ReadBe32(const uint8_t* bytes) {
    return (static_cast<uint32_t>(bytes[0]) << 24) | (static_cast<uint32_t>(bytes[1]) << 16) |
        (static_cast<uint32_t>(bytes[2]) << 8) | bytes[3];
}

uint32_t ReadLe32(const uint8_t* bytes) {
    return bytes[0] | (static_cast<uint32_t>(bytes[1]) << 8) |
        (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
}

bool DecodePngHeader(const uint8_t* data, uint32_t size, uint32_t& width, uint32_t& height) {
    static const uint8_t signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (!data || size < 24) return false;
    for (uint32_t i = 0; i < 8; i++) if (data[i] != signature[i]) return false;
    if (data[12] != 'I' || data[13] != 'H' || data[14] != 'D' || data[15] != 'R') return false;
    width = ReadBe32(data + 16); height = ReadBe32(data + 20);
    return width != 0 && height != 0;
}

bool DecodeBmpHeader(const uint8_t* data, uint32_t size, uint32_t& width, uint32_t& height) {
    if (!data || size < 26 || data[0] != 'B' || data[1] != 'M') return false;
    width = ReadLe32(data + 18); height = ReadLe32(data + 22);
    return width != 0 && height != 0;
}

bool DecodeJpegHeader(const uint8_t* data, uint32_t size) {
    return data && size >= 4 && data[0] == 0xFF && data[1] == 0xD8 && data[size - 2] == 0xFF && data[size - 1] == 0xD9;
}

bool DecodeSvgDocument(const char* data) {
    if (!data) return false;
    while (*data == ' ' || *data == '\n' || *data == '\t') data++;
    return data[0] == '<' && data[1] == 's' && data[2] == 'v' && data[3] == 'g';
}

struct FontCacheEntry { uint32_t codepoint; uint8_t pixels[8]; bool valid; };
FontCacheEntry g_FontCache[kFontCacheEntries];

bool LoadTrueTypeFont(const uint8_t* data, uint32_t size) {
    if (!data || size < 12) return false;
    const bool sfnt = data[0] == 0 && data[1] == 1 && data[2] == 0 && data[3] == 0;
    const bool otto = data[0] == 'O' && data[1] == 'T' && data[2] == 'T' && data[3] == 'O';
    return sfnt || otto;
}

bool CacheFontGlyph(uint32_t codepoint, const uint8_t pixels[8]) {
    for (uint32_t i = 0; i < kFontCacheEntries; i++) {
        if (g_FontCache[i].valid && g_FontCache[i].codepoint == codepoint) return true;
        if (!g_FontCache[i].valid) {
            g_FontCache[i].codepoint = codepoint;
            for (uint32_t row = 0; row < 8; row++) g_FontCache[i].pixels[row] = pixels[row];
            g_FontCache[i].valid = true;
            return true;
        }
    }
    return false;
}

bool RunDesktopPhaseSelfTest() {
    Window saved; CopyWindow(saved, g_Windows[0]);
    const bool resize_ok = ResizeWindow(g_Windows[0], 300, 180);
    const bool snap_ok = SnapWindow(g_Windows[0], SnapEdge::Left);
    const bool desktops_ok = SwitchVirtualDesktop(1) && SwitchVirtualDesktop(0);
    CopyWindow(g_Windows[0], saved);

    uint8_t png[24] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13, 'I', 'H', 'D', 'R', 0, 0, 0, 2, 0, 0, 0, 3};
    uint8_t bmp[26] = {'B', 'M'}; bmp[18] = 2; bmp[22] = 3;
    const uint8_t jpeg[4] = {0xFF, 0xD8, 0xFF, 0xD9};
    const uint8_t ttf[12] = {0, 1, 0, 0};
    uint32_t width = 0, height = 0;
    uint8_t glyph[8] = {0x18, 0x24, 0x42, 0x7E, 0x42, 0x42, 0, 0};
    return resize_ok && snap_ok && desktops_ok &&
        DecodePngHeader(png, sizeof(png), width, height) && width == 2 && height == 3 &&
        DecodeBmpHeader(bmp, sizeof(bmp), width, height) && width == 2 && height == 3 &&
        DecodeJpegHeader(jpeg, sizeof(jpeg)) && DecodeSvgDocument("<svg></svg>") &&
        LoadTrueTypeFont(ttf, sizeof(ttf)) && CacheFontGlyph('A', glyph);
}

bool RunApplicationsSelfTest() {
    Window saved_windows[kMaxWindows];
    AppRuntimeState saved_runtimes[kApplicationCapacity];
    for (uint32_t i = 0; i < kMaxWindows; i++) {
        CopyWindow(saved_windows[i], g_Windows[i]);
    }
    for (uint32_t i = 0; i < kApplicationCapacity; i++) {
        saved_runtimes[i] = g_AppRuntimes[i];
    }
    const uint32_t saved_window_count = g_Status.window_count;
    const uint8_t saved_desktop = g_ActiveDesktop;
    const int32_t saved_active = g_ActiveWindowIndex;
    const char* saved_notification = g_NotificationText;
    const uint32_t saved_notification_color = g_NotificationColor;

    const bool registry_ok =
        ApplicationRegistered(AppKind::FileManager) &&
        ApplicationRegistered(AppKind::TextEditor) &&
        ApplicationRegistered(AppKind::ImageViewer) &&
        ApplicationRegistered(AppKind::Calculator) &&
        ApplicationRegistered(AppKind::Settings) &&
        ApplicationRegistered(AppKind::TaskManager) &&
        ApplicationRegistered(AppKind::PackageManager) &&
        ApplicationRegistered(AppKind::SystemMonitor) &&
        ApplicationRegistered(AppKind::TerminalEmulator) &&
        ApplicationRegistered(AppKind::SoftwareCenter);
    const bool descriptors_ok =
        FindApplication(AppKind::FileManager) &&
        FindApplication(AppKind::TextEditor) &&
        FindApplication(AppKind::ImageViewer) &&
        FindApplication(AppKind::Calculator) &&
        FindApplication(AppKind::Settings) &&
        FindApplication(AppKind::TaskManager) &&
        FindApplication(AppKind::PackageManager) &&
        FindApplication(AppKind::SystemMonitor) &&
        FindApplication(AppKind::TerminalEmulator) &&
        FindApplication(AppKind::SoftwareCenter);
    const bool paths_ok =
        FindApplication(AppKind::FileManager)->executable_path[0] == '/' &&
        FindApplication(AppKind::FileManager)->icon_path[0] == '/' &&
        FindApplication(AppKind::TerminalEmulator)->executable_path[0] == '/' &&
        FindApplication(AppKind::TerminalEmulator)->icon_path[0] == '/';

    bool launch_all_ok = true;
    for (uint32_t i = 0; i < kApplicationCount; i++) {
        launch_all_ok = LaunchApplicationById(kApplications[i].id) && launch_all_ok;
    }

    bool taskbar_running_ok = true;
    for (uint32_t i = 0; i < kApplicationCount; i++) {
        const AppRuntimeState* runtime = FindAppRuntime(kApplications[i].app);
        taskbar_running_ok = taskbar_running_ok &&
            runtime &&
            runtime->state == AppLifecycleState::Running &&
            FindWindowByApp(kApplications[i].app, kApplications[i].desktop) >= 0;
    }
    const bool missing_id_ok = !LaunchApplicationById("missing") &&
        g_NotificationText &&
        g_NotificationColor == kTheme.close;

    const ApplicationDescriptor* terminal = FindApplication(AppKind::TerminalEmulator);
    const int32_t terminal_index = terminal ? FindWindowByApp(terminal->app, terminal->desktop) : -1;
    bool restore_ok = false;
    bool close_relaunch_ok = false;
    bool watchdog_ok = false;
    if (terminal && terminal_index >= 0) {
        MinimizeWindow(static_cast<uint32_t>(terminal_index));
        restore_ok = LaunchApplicationById(terminal->id) &&
            FindWindowByApp(terminal->app, terminal->desktop) >= 0 &&
            !g_Windows[FindWindowByApp(terminal->app, terminal->desktop)].minimized;

        const int32_t close_index = FindWindowByApp(terminal->app, terminal->desktop);
        CloseWindow(static_cast<uint32_t>(close_index));
        close_relaunch_ok = LaunchApplicationById(terminal->id) &&
            FindWindowByApp(terminal->app, terminal->desktop) >= 0 &&
            g_Windows[FindWindowByApp(terminal->app, terminal->desktop)].visible;

        AppRuntimeState* runtime = FindAppRuntime(terminal->app);
        if (runtime) {
            runtime->state = AppLifecycleState::Running;
            runtime->missed_heartbeat_count = 2;
            AppWatchdogTick();
            watchdog_ok = runtime->state == AppLifecycleState::NotResponding;
            AppHeartbeat(terminal->app);
        }
    }

    for (uint32_t i = 0; i < kMaxWindows; i++) {
        CopyWindow(g_Windows[i], saved_windows[i]);
    }
    for (uint32_t i = 0; i < kApplicationCapacity; i++) {
        g_AppRuntimes[i] = saved_runtimes[i];
    }
    g_Status.window_count = saved_window_count;
    g_ActiveDesktop = saved_desktop;
    g_ActiveWindowIndex = saved_active;
    g_NotificationText = saved_notification;
    g_NotificationColor = saved_notification_color;

    return registry_ok &&
        descriptors_ok &&
        paths_ok &&
        launch_all_ok &&
        taskbar_running_ok &&
        missing_id_ok &&
        restore_ok &&
        close_relaunch_ok &&
        watchdog_ok;
}

bool RunStableInterfaceSelfTest() {
    Window saved_windows[kMaxWindows];
    AppRuntimeState saved_runtimes[kApplicationCapacity];
    for (uint32_t i = 0; i < kMaxWindows; i++) {
        CopyWindow(saved_windows[i], g_Windows[i]);
    }
    for (uint32_t i = 0; i < kApplicationCapacity; i++) {
        saved_runtimes[i] = g_AppRuntimes[i];
    }
    const uint32_t saved_window_count = g_Status.window_count;
    const uint8_t saved_desktop = g_ActiveDesktop;
    const int32_t saved_active = g_ActiveWindowIndex;
    const bool saved_full_redraw = g_DebugFullRedraw;
    const char* saved_notification = g_NotificationText;
    const uint32_t saved_notification_color = g_NotificationColor;

    g_DebugFullRedraw = true;
    const bool switched = SwitchVirtualDesktop(0);
    const int32_t file_index = FindWindowByApp(AppKind::FileManager, 0);
    const int32_t terminal_index = FindWindowByApp(AppKind::TerminalEmulator, 0);
    const bool focus_file_ok = file_index >= 0 && FocusWindow(static_cast<uint32_t>(file_index));
    const bool focus_terminal_ok = terminal_index >= 0 && FocusWindow(static_cast<uint32_t>(FindWindowByApp(AppKind::TerminalEmulator, 0)));
    const bool active_terminal_ok = g_ActiveWindowIndex >= 0 &&
        g_Windows[g_ActiveWindowIndex].focused &&
        g_Windows[g_ActiveWindowIndex].app == AppKind::TerminalEmulator;
    const bool previous_unfocused_ok = FindWindowByApp(AppKind::FileManager, 0) >= 0 &&
        !g_Windows[FindWindowByApp(AppKind::FileManager, 0)].focused;

    const uint32_t before_minimize_count = g_Status.window_count;
    MinimizeWindow(static_cast<uint32_t>(g_ActiveWindowIndex));
    const bool minimize_persistent_ok = g_Status.window_count == before_minimize_count &&
        FindWindowByApp(AppKind::TerminalEmulator, 0) >= 0;
    RestoreWindowFromTaskbar(static_cast<uint32_t>(FindWindowByApp(AppKind::TerminalEmulator, 0)));

    const int32_t active_before_close = g_ActiveWindowIndex;
    CloseWindow(static_cast<uint32_t>(active_before_close));
    const int32_t terminal_after_close = FindWindowByApp(AppKind::TerminalEmulator, 0);
    const bool close_persistent_ok = g_Status.window_count == before_minimize_count &&
        terminal_after_close >= 0 &&
        !g_Windows[terminal_after_close].visible;

    RedrawDirtyRegion({0, 0, 0, 0});
    const bool full_redraw_fallback_ok = g_DebugFullRedraw && g_BackBufferReady;

    for (uint32_t i = 0; i < kMaxWindows; i++) {
        CopyWindow(g_Windows[i], saved_windows[i]);
    }
    for (uint32_t i = 0; i < kApplicationCapacity; i++) {
        g_AppRuntimes[i] = saved_runtimes[i];
    }
    g_Status.window_count = saved_window_count;
    g_ActiveDesktop = saved_desktop;
    g_ActiveWindowIndex = saved_active;
    g_DebugFullRedraw = saved_full_redraw;
    g_NotificationText = saved_notification;
    g_NotificationColor = saved_notification_color;

    return switched &&
        focus_file_ok &&
        focus_terminal_ok &&
        active_terminal_ok &&
        previous_unfocused_ok &&
        minimize_persistent_ok &&
        close_persistent_ok &&
        full_redraw_fallback_ok;
}

bool RunWindowPlacementSelfTest() {
    AppPlacementState saved_placements[kMaxWindows];
    for (uint32_t i = 0; i < kMaxWindows; i++) {
        saved_placements[i] = g_AppPlacements[i];
        g_AppPlacements[i].valid = false;
    }

    const Rect usable = UsableDesktopRect();
    const ApplicationDescriptor* files = FindApplication(AppKind::FileManager);
    const ApplicationDescriptor* terminal = FindApplication(AppKind::TerminalEmulator);
    const ApplicationDescriptor* editor = FindApplication(AppKind::TextEditor);
    const ApplicationDescriptor* calculator = FindApplication(AppKind::Calculator);
    const ApplicationDescriptor* settings = FindApplication(AppKind::Settings);
    if (!files || !terminal || !editor || !calculator || !settings) {
        for (uint32_t i = 0; i < kMaxWindows; i++) {
            g_AppPlacements[i] = saved_placements[i];
        }
        return false;
    }

    const Rect first = InitialWindowBounds(*files, 0);
    const uint32_t expected_x = usable.width > first.width ? usable.x + (usable.width - first.width) / 2 : usable.x;
    const uint32_t expected_y = usable.height > first.height ? usable.y + (usable.height - first.height) / 2 : usable.y;
    const bool first_centered_ok = first.x == expected_x && first.y == expected_y;

    Rect tiny = {0, 0, 20, 20};
    const Rect clamped_files = ClampWindowToUsableArea(tiny, AppKind::FileManager);
    const bool default_min_ok = clamped_files.width >= kDefaultAppMinWidth &&
        clamped_files.height >= kDefaultAppMinHeight &&
        clamped_files.y >= usable.y &&
        clamped_files.y + clamped_files.height <= usable.y + usable.height;

    const Rect clamped_calc = ClampWindowToUsableArea(tiny, AppKind::Calculator);
    const bool compact_ok = calculator->compact &&
        settings->compact &&
        clamped_calc.width >= kCompactAppMinWidth &&
        clamped_calc.height >= kCompactAppMinHeight;

    const Rect terminal_bounds = InitialWindowBounds(*terminal, 0);
    const Rect editor_bounds = InitialWindowBounds(*editor, 0);
    const bool terminal_editor_ok = terminal_bounds.width >= terminal->min_width &&
        terminal_bounds.height >= terminal->min_height &&
        editor_bounds.width >= editor->min_width &&
        editor_bounds.height >= editor->min_height;

    const Rect offscreen = {g_Framebuffer.width + 200, g_Framebuffer.height + 200, 900, 700};
    const Rect visible = ClampWindowToUsableArea(offscreen, AppKind::TextEditor);
    const bool visible_ok = visible.x + visible.width <= usable.x + usable.width &&
        visible.y + visible.height <= usable.y + usable.height;

    const Rect remembered = {usable.x + 40, usable.y + 44, files->min_width, files->min_height};
    RememberAppPlacement(AppKind::FileManager, remembered);
    const Rect restored = InitialWindowBounds(*files, 0);
    const bool remembered_ok = restored.x == remembered.x &&
        restored.y == remembered.y &&
        restored.width == remembered.width &&
        restored.height == remembered.height;

    for (uint32_t i = 0; i < kMaxWindows; i++) {
        g_AppPlacements[i] = saved_placements[i];
    }

    return first_centered_ok &&
        default_min_ok &&
        compact_ok &&
        terminal_editor_ok &&
        visible_ok &&
        remembered_ok;
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

void TerminalCopyText(char* destination, uint32_t capacity, const char* source) {
    if (!destination || capacity == 0) {
        return;
    }

    uint32_t i = 0;
    if (source) {
        for (; i + 1 < capacity && source[i]; i++) {
            destination[i] = source[i];
        }
    }
    destination[i] = '\0';
}

bool TerminalTextEmpty(const char* text) {
    return !text || text[0] == '\0';
}

uint32_t TerminalColorFromAnsiCode(uint32_t code, uint32_t fallback) {
    switch (code) {
        case 0: return kTheme.text;
        case 30: return 0xFF1E252E;
        case 31: return kTheme.close;
        case 32: return kTheme.maximize;
        case 33: return kTheme.minimize;
        case 34: return kTheme.border_active;
        case 35: return 0xFFC678DD;
        case 36: return kTheme.accent;
        case 37: return kTheme.text;
        default: return fallback;
    }
}

bool TerminalParseAnsiLine(const char* text, char* stripped, uint32_t capacity, uint32_t& color) {
    if (!text || !stripped || capacity == 0) {
        return false;
    }

    bool parsed_escape = false;
    uint32_t out = 0;
    for (uint32_t i = 0; text[i] && out + 1 < capacity; i++) {
        if (text[i] == 0x1B && text[i + 1] == '[') {
            i += 2;
            uint32_t code = 0;
            bool has_digit = false;
            while (text[i] >= '0' && text[i] <= '9') {
                has_digit = true;
                code = code * 10 + static_cast<uint32_t>(text[i] - '0');
                i++;
            }
            if (text[i] == 'm') {
                color = TerminalColorFromAnsiCode(has_digit ? code : 0, color);
                parsed_escape = true;
                continue;
            }
            if (!text[i]) {
                break;
            }
        }

        stripped[out++] = text[i];
    }
    stripped[out] = '\0';
    return parsed_escape;
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

    uint32_t line_color = color;
    if (!TerminalParseAnsiLine(text, g_TerminalLines[g_TerminalLineCount].text,
            sizeof(g_TerminalLines[g_TerminalLineCount].text), line_color)) {
        TerminalCopyText(g_TerminalLines[g_TerminalLineCount].text,
            sizeof(g_TerminalLines[g_TerminalLineCount].text), text);
    }
    g_TerminalLines[g_TerminalLineCount].color = line_color;
    g_TerminalLineCount++;

    const int32_t max_scroll = g_TerminalLineCount > kVisibleTerminalLines
        ? static_cast<int32_t>(g_TerminalLineCount - kVisibleTerminalLines)
        : 0;
    if (g_TerminalScrollOffset > max_scroll) {
        g_TerminalScrollOffset = max_scroll;
    }
}

void TerminalClear() {
    g_TerminalLineCount = 0;
    g_TerminalScrollOffset = 0;
    for (uint32_t i = 0; i < kMaxTerminalLines; i++) {
        g_TerminalLines[i].text[0] = '\0';
        g_TerminalLines[i].color = 0;
    }
}

void TerminalRecordHistory(const char* command) {
    if (TerminalTextEmpty(command)) {
        return;
    }

    if (g_TerminalHistoryCount >= kTerminalHistoryEntries) {
        for (uint32_t i = 1; i < kTerminalHistoryEntries; i++) {
            TerminalCopyText(g_TerminalHistory[i - 1], sizeof(g_TerminalHistory[i - 1]), g_TerminalHistory[i]);
        }
        g_TerminalHistoryCount = kTerminalHistoryEntries - 1;
    }

    TerminalCopyText(g_TerminalHistory[g_TerminalHistoryCount],
        sizeof(g_TerminalHistory[g_TerminalHistoryCount]), command);
    g_TerminalHistoryCount++;
    g_TerminalHistoryCursor = static_cast<int32_t>(g_TerminalHistoryCount);
}

const char* TerminalRecallHistory(int32_t delta) {
    if (g_TerminalHistoryCount == 0) {
        return "";
    }

    if (g_TerminalHistoryCursor < 0) {
        g_TerminalHistoryCursor = static_cast<int32_t>(g_TerminalHistoryCount);
    }

    g_TerminalHistoryCursor += delta;
    if (g_TerminalHistoryCursor < 0) {
        g_TerminalHistoryCursor = 0;
    }
    if (g_TerminalHistoryCursor >= static_cast<int32_t>(g_TerminalHistoryCount)) {
        g_TerminalHistoryCursor = static_cast<int32_t>(g_TerminalHistoryCount - 1);
    }

    return g_TerminalHistory[g_TerminalHistoryCursor];
}

const char* TerminalAutocompleteCommand(const char* prefix) {
    static constexpr const char* kCommands[] = {
        "help", "clear", "mem", "cpu", "version", "uptime",
        "ls", "pwd", "cd /", "cd /boot", "cd /tmp", "cat readme",
        "reboot", "shutdown", "color demo", "script demo"
    };

    for (uint32_t i = 0; i < sizeof(kCommands) / sizeof(kCommands[0]); i++) {
        if (TerminalStartsWith(kCommands[i], prefix)) {
            return kCommands[i];
        }
    }

    return prefix;
}

bool ExecuteTerminalScript(const char* script);

bool ExecuteTerminalCommand(const char* command) {
    if (!command) {
        return false;
    }

    if (!TerminalTextEquals(command, "history-prev") && !TerminalStartsWith(command, "complete ")) {
        TerminalRecordHistory(command);
    }

    if (TerminalTextEquals(command, "help")) {
        TerminalAppendLine("help mem cpu clear version uptime", kTheme.accent);
        TerminalAppendLine("ls pwd cd cat reboot shutdown color demo", kTheme.accent);
        TerminalAppendLine("complete <prefix> history-prev script demo", kTheme.accent);
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
    if (TerminalTextEquals(command, "color demo")) {
        TerminalAppendLine("\x1B[31mred \x1B[32mgreen \x1B[36mcyan", kTheme.text);
        return true;
    }
    if (TerminalTextEquals(command, "history-prev")) {
        TerminalAppendLine(TerminalRecallHistory(-1), kTheme.text_muted);
        return true;
    }
    if (TerminalStartsWith(command, "complete ")) {
        TerminalAppendLine(TerminalAutocompleteCommand(command + 9), kTheme.text);
        return true;
    }
    if (TerminalTextEquals(command, "script demo")) {
        TerminalAppendLine("script: mem;cpu;uptime", kTheme.text_muted);
        return ExecuteTerminalScript("mem;cpu;uptime");
    }

    TerminalAppendLine("shell: unknown command", kTheme.close);
    return false;
}

bool ExecuteTerminalScript(const char* script) {
    if (!script) {
        return false;
    }

    bool all_ok = true;
    char command[kTerminalCommandLength];
    uint32_t command_length = 0;

    for (uint32_t i = 0;; i++) {
        const char c = script[i];
        if (c == ';' || c == '\0') {
            command[command_length] = '\0';
            if (command_length > 0 && !ExecuteTerminalCommand(command)) {
                all_ok = false;
            }
            command_length = 0;
            if (c == '\0') {
                break;
            }
            continue;
        }

        if (command_length + 1 < sizeof(command)) {
            command[command_length++] = c;
        }
    }

    return all_ok;
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
    g_TerminalHistoryCount = 0;
    g_TerminalHistoryCursor = -1;
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
    const bool history_ok = TerminalTextEquals(TerminalRecallHistory(-1), "shutdown");
    const bool complete_ok = TerminalTextEquals(TerminalAutocompleteCommand("ver"), "version");

    TerminalAppendLine("\x1B[31mansi color");
    const bool ansi_ok = g_TerminalLineCount > 0 &&
        TerminalTextEquals(g_TerminalLines[g_TerminalLineCount - 1].text, "ansi color");
    const bool colors_ok = g_TerminalLineCount > 0 &&
        g_TerminalLines[g_TerminalLineCount - 1].color == kTheme.close;

    TerminalClear();
    for (uint32_t i = 0; i < kVisibleTerminalLines + 3; i++) {
        TerminalAppendLine("scrollback entry", kTheme.text);
    }
    g_TerminalScrollOffset = 2;
    const bool scrollback_ok = g_TerminalLineCount == kVisibleTerminalLines + 3 &&
        g_TerminalScrollOffset == 2;

    TerminalClear();
    const bool scripting_ok = ExecuteTerminalScript("mem;cpu;uptime") && g_TerminalLineCount == 3;

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
        shutdown_ok &&
        ansi_ok &&
        colors_ok &&
        scrollback_ok &&
        history_ok &&
        complete_ok &&
        scripting_ok;
}

void DrawTerminalContents(const Window& window) {
    const uint32_t x = window.bounds.x + 18;
    uint32_t y = window.bounds.y + kTitleBarHeight + 18;
    const uint32_t visible_count = g_TerminalLineCount < kVisibleTerminalLines
        ? g_TerminalLineCount
        : kVisibleTerminalLines;
    const uint32_t scrollback = g_TerminalLineCount > visible_count
        ? g_TerminalLineCount - visible_count
        : 0;
    uint32_t start = scrollback;

    if (g_TerminalScrollOffset > 0) {
        const uint32_t offset = static_cast<uint32_t>(g_TerminalScrollOffset);
        start = offset > scrollback ? 0 : scrollback - offset;
    }

    for (uint32_t i = start; i < g_TerminalLineCount; i++) {
        if (g_TerminalLines[i].text[0]) {
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

void DrawApplicationContents(const Window& window) {
    const uint32_t x = window.bounds.x + 18;
    uint32_t y = window.bounds.y + kTitleBarHeight + 18;

    switch (window.app) {
        case AppKind::FileManager:
            Graphics::DrawText(x, y, "/  boot  kernel  tmp  readme", kTheme.text);
            Graphics::DrawRect({x, y + 28, 180, 74}, kTheme.border_active);
            Graphics::DrawText(x + 12, y + 44, "Files ready", kTheme.text_muted);
            break;
        case AppKind::TextEditor:
            Graphics::DrawText(x, y, "readme.txt", kTheme.accent);
            Graphics::DrawRect({x, y + 26, 220, 90}, kTheme.border_inactive);
            Graphics::DrawText(x + 10, y + 44, "Antigravity OS notes", kTheme.text);
            break;
        case AppKind::ImageViewer:
            Graphics::DrawText(x, y, "SO.png", kTheme.text);
            Graphics::FillRect({x, y + 26, 150, 84}, 0xFF1E2D34);
            Graphics::DrawImage({x + 18, y + 38, 114, 60}, kTheme.accent);
            break;
        case AppKind::Calculator:
            Graphics::DrawText(x, y, "42", kTheme.text);
            for (uint32_t row = 0; row < 3; row++) {
                for (uint32_t col = 0; col < 4; col++) {
                    Graphics::DrawRect({x + col * 34, y + 30 + row * 28, 26, 20}, kTheme.border_inactive);
                }
            }
            break;
        case AppKind::Settings:
            Graphics::DrawText(x, y, "Display  Network  Security", kTheme.text);
            Graphics::FillRect({x, y + 30, 130, 10}, kTheme.accent);
            Graphics::FillRect({x, y + 54, 96, 10}, kTheme.minimize);
            break;
        case AppKind::TaskManager:
            Graphics::DrawText(x, y, "PID  Name        State", kTheme.text_muted);
            Graphics::DrawText(x, y + 24, "1    kernel      running", kTheme.text);
            Graphics::DrawText(x, y + 48, "2    shell       ready", kTheme.text);
            break;
        case AppKind::PackageManager:
            Graphics::DrawText(x, y, "base  gui  net  dev", kTheme.text);
            Graphics::DrawText(x, y + 28, "updates: none", kTheme.maximize);
            break;
        case AppKind::SystemMonitor:
            DrawSystemMonitorContents(window);
            break;
        case AppKind::TerminalEmulator:
            DrawTerminalContents(window);
            break;
        case AppKind::SoftwareCenter:
            Graphics::DrawText(x, y, "Installed: Terminal Files Settings", kTheme.text);
            Graphics::DrawText(x, y + 28, "Catalog cache ready", kTheme.accent);
            break;
    }
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
    if (!window.visible || window.minimized || window.desktop != g_ActiveDesktop) {
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

    DrawApplicationContents(window);
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
    if (g_NotificationText) {
        Graphics::DrawText(508, 12, g_NotificationText, g_NotificationColor);
    }
    if (g_Framebuffer.width > 92) {
        Graphics::DrawText(g_Framebuffer.width - 82, 12, "22:45", kTheme.text);
    }

    uint32_t x = 18;
    const uint32_t y = g_Framebuffer.height - kTaskBarHeight + 8;
    for (uint32_t i = 0; i < kApplicationCount && x + kLauncherButtonWidth < g_Framebuffer.width; i++) {
        AppRuntimeState* runtime = FindAppRuntime(kApplications[i].app);
        const bool running = runtime && runtime->state == AppLifecycleState::Running;
        const bool failed = runtime && runtime->state == AppLifecycleState::Failed;
        const uint32_t fill = failed ? 0xFF4A2630 : (running ? 0xFF243A34 : 0xFF26313D);
        const uint32_t edge = failed ? kTheme.close : (running ? kTheme.maximize : kTheme.border_inactive);
        Graphics::FillRect({x, y, kLauncherButtonWidth, kLauncherButtonHeight}, fill);
        Graphics::DrawRect({x, y, kLauncherButtonWidth, kLauncherButtonHeight}, edge);
        Graphics::DrawText(x + 8, y + 7, kApplications[i].id, running ? kTheme.text : kTheme.text_muted);
        x += kLauncherButtonWidth + 8;
    }
}

void DrawTaskbar() {
    uint32_t x = 18;
    const uint32_t y = g_Framebuffer.height - kTaskBarHeight + 40;

    for (uint32_t i = 0; i < g_Status.window_count; i++) {
        const Window& window = g_Windows[i];
        if (!window.visible || window.desktop != g_ActiveDesktop) {
            continue;
        }

        const uint32_t button_width = window.app == AppKind::TerminalEmulator ? 112 : 156;
        const uint32_t fill = window.focused ? 0xFF345164 : (window.minimized ? 0xFF1C242E : 0xFF2D3945);
        const uint32_t edge = window.focused ? kTheme.accent : (window.minimized ? kTheme.border_inactive : kTheme.border_active);
        Graphics::FillRect({x, y, button_width, 26}, fill);
        Graphics::DrawRect({x, y, button_width, 26}, edge);
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

    for (uint32_t i = 0; i < g_Status.window_count; i++) {
        ComposeWindow(g_Windows[i], g_Windows[i].focused);
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
    if (!g_BackBufferReady || g_DebugFullRedraw) {
        ComposeDesktop();
        return;
    }

    const Rect clipped = IntersectRect(dirty, ScreenRect());
    if (RectEmpty(clipped)) {
        ComposeDesktop();
        return;
    }

    BeginDrawToBackBuffer(clipped);
    DrawWallpaper();
    DrawDesktopIcons();
    DrawBars();

    for (uint32_t i = 0; i < g_Status.window_count; i++) {
        ComposeWindow(g_Windows[i], g_Windows[i].focused);
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

    AppRuntimeState* runtime = FindAppRuntime(g_Windows[index].app);
    if (runtime && runtime->process_id == g_Windows[index].process_id) {
        KernelExitUserApplication(runtime->process_id, 0);
        runtime->state = AppLifecycleState::NotRunning;
        runtime->process_id = 0;
        runtime->thread_id = 0;
        runtime->missed_heartbeat_count = 0;
    }
    g_Windows[index].visible = false;
    g_Windows[index].focused = false;
    if (g_DragWindowIndex == static_cast<int32_t>(index)) {
        g_DragWindowIndex = -1;
    }
    if (g_MouseDownWindowIndex == static_cast<int32_t>(index)) {
        g_MouseDownWindowIndex = -1;
    }
    if (g_ActiveWindowIndex == static_cast<int32_t>(index)) {
        FocusTopVisibleWindow();
    }
}

int32_t FindTaskbarWindowAt(int32_t x, int32_t y) {
    if (y < static_cast<int32_t>(g_Framebuffer.height - kTaskBarHeight + 36)) {
        return -1;
    }

    int32_t button_x = 18;
    const int32_t button_y = static_cast<int32_t>(g_Framebuffer.height - kTaskBarHeight + 40);
    for (uint32_t i = 0; i < g_Status.window_count; i++) {
        const Window& window = g_Windows[i];
        if (!window.visible || window.desktop != g_ActiveDesktop) {
            continue;
        }

        const int32_t button_width = window.app == AppKind::TerminalEmulator ? 112 : 156;
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

const ApplicationDescriptor* FindLauncherAppAt(int32_t x, int32_t y) {
    const int32_t launcher_y = static_cast<int32_t>(g_Framebuffer.height - kTaskBarHeight + 8);
    if (y < launcher_y || y >= launcher_y + static_cast<int32_t>(kLauncherButtonHeight)) {
        return nullptr;
    }

    int32_t button_x = 18;
    for (uint32_t i = 0; i < kApplicationCount; i++) {
        if (PointInRect(x, y, {
            static_cast<uint32_t>(button_x),
            static_cast<uint32_t>(launcher_y),
            kLauncherButtonWidth,
            kLauncherButtonHeight,
        })) {
            return &kApplications[i];
        }
        button_x += static_cast<int32_t>(kLauncherButtonWidth + 8);
    }

    return nullptr;
}

void UpdateHoverState() {
    g_HoveredWindowIndex = FindTopWindowAt(g_MouseX, g_MouseY, false);
    g_HoveredControl = WindowControl::None;
    g_CursorKind = CursorKind::Arrow;

    if (FindLauncherAppAt(g_MouseX, g_MouseY) || FindTaskbarWindowAt(g_MouseX, g_MouseY) >= 0) {
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

    const ApplicationDescriptor* launcher_app = FindLauncherAppAt(x, y);
    if (launcher_app) {
        LaunchApplicationById(launcher_app->id);
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

    const WindowControl control = HitWindowControl(x, y, g_Windows[window_index]);
    FocusWindow(static_cast<uint32_t>(window_index));
    const int32_t focused_index = g_ActiveWindowIndex;
    if (focused_index < 0) {
        return;
    }

    if (control == WindowControl::Close) {
        CloseWindow(static_cast<uint32_t>(focused_index));
    } else if (control == WindowControl::Minimize) {
        MinimizeWindow(static_cast<uint32_t>(focused_index));
    } else if (control == WindowControl::Maximize) {
        ToggleMaximizeWindow(static_cast<uint32_t>(focused_index));
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
                    FocusWindow(static_cast<uint32_t>(front_window));
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
            if (g_Dragging && g_DragWindowIndex >= 0) {
                if (g_MouseY <= static_cast<int32_t>(kTopBarHeight + 8))
                    SnapWindow(g_Windows[g_DragWindowIndex], SnapEdge::Top);
                else if (g_MouseX <= 8)
                    SnapWindow(g_Windows[g_DragWindowIndex], SnapEdge::Left);
                else if (g_MouseX >= static_cast<int32_t>(g_Framebuffer.width - 8))
                    SnapWindow(g_Windows[g_DragWindowIndex], SnapEdge::Right);
            }
            g_LeftButtonDown = false;
            g_DragWindowIndex = -1;
            g_MouseDownWindowIndex = -1;
            g_Dragging = false;
            UpdateHoverState();
            ComposeDesktop();
            break;

        case GuiEventType::MouseWheel:
            // Wheel delta is carried as a signed value in key for scrollable clients.
            if (g_ActiveWindowIndex >= 0 &&
                g_Windows[g_ActiveWindowIndex].app == AppKind::TerminalEmulator) {
                g_TerminalScrollOffset += static_cast<int32_t>(event.key);
                if (g_TerminalScrollOffset < 0) g_TerminalScrollOffset = 0;
                if (g_TerminalLineCount > kVisibleTerminalLines) {
                    const int32_t max_scroll = static_cast<int32_t>(g_TerminalLineCount - kVisibleTerminalLines);
                    if (g_TerminalScrollOffset > max_scroll) g_TerminalScrollOffset = max_scroll;
                } else {
                    g_TerminalScrollOffset = 0;
                }
            }
            UpdateHoverState();
            break;

        case GuiEventType::Gamepad:
            // Axis/button state is queued for the focused application.
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
            ComposeDesktop();
            break;

        case GuiEventType::Resize: {
            const int32_t index = FindTopWindowAt(event.x, event.y, false);
            if (index >= 0) ResizeWindow(g_Windows[index], event.button, event.key);
            ComposeDesktop();
            break;
        }

        case GuiEventType::DoubleClick: {
            const int32_t index = FindTopWindowAt(event.x, event.y, true);
            if (index >= 0) ToggleMaximizeWindow(static_cast<uint32_t>(index));
            ComposeDesktop();
            break;
        }

        case GuiEventType::KeyDown:
            if (event.key >= '1' && event.key <= '4' && SwitchVirtualDesktop(static_cast<uint8_t>(event.key - '1')))
                ComposeDesktop();
            break;

        case GuiEventType::None:
        case GuiEventType::Hover:
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
        KernelLog(LogLevel::Warn, "GUI window manager init failed");
        return false;
    }

    if (!RunDesktopPhaseSelfTest()) {
        KernelLog(LogLevel::Warn, "Desktop/window/image/font self-test failed");
        return false;
    }

    if (!RunApplicationsSelfTest()) {
        KernelLog(LogLevel::Warn, "Application registry self-test failed");
        return false;
    }

    if (!RunStableInterfaceSelfTest()) {
        KernelLog(LogLevel::Warn, "Stable interface self-test failed");
        return false;
    }

    if (!RunWindowPlacementSelfTest()) {
        KernelLog(LogLevel::Warn, "Window placement self-test failed");
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

void KernelGuiRenderNow() {
    if (g_Status.window_manager_ready) {
        g_Status.compositor_ready = ComposeDesktop();
        g_Status.desktop_ready = g_Status.compositor_ready && g_Status.window_manager_ready;
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
