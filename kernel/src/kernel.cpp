#include "kernel.hpp"
#include "cpu.hpp"
#include "scheduler.hpp"

// Simple 8x8 font bitmap for the kernel printout (basic printable ASCII subset)
static const uint8_t font8x8_kernel[128][8] = {
    {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
    {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Space
    {0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x18, 0x00}, // !
    {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // "
    {0x36, 0x7f, 0x36, 0x36, 0x7f, 0x36, 0x36, 0x00}, // #
    {0x18, 0x3e, 0x60, 0x3c, 0x06, 0x7c, 0x18, 0x00}, // $
    {0x00, 0x66, 0x30, 0x18, 0x0c, 0x66, 0x00, 0x00}, // %
    {0x38, 0x6c, 0x38, 0x76, 0xdc, 0xcc, 0x76, 0x00}, // &
    {0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00}, // '
    {0x0c, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0c, 0x00}, // (
    {0x30, 0x18, 0x0c, 0x0c, 0x0c, 0x18, 0x30, 0x00}, // )
    {0x00, 0x66, 0x3c, 0xff, 0x3c, 0x66, 0x00, 0x00}, // *
    {0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x00}, // +
    {0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x08, 0x10}, // ,
    {0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00}, // -
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00}, // .
    {0x00, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x00}, // /
    // 0 - 9
    {0x3c, 0x66, 0x6e, 0x76, 0x66, 0x66, 0x3c, 0x00}, // 0
    {0x18, 0x1c, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00}, // 1
    {0x3e, 0x66, 0x06, 0x1c, 0x30, 0x62, 0x7e, 0x00}, // 2
    {0x3e, 0x66, 0x06, 0x1e, 0x06, 0x66, 0x3e, 0x00}, // 3
    {0x06, 0x0e, 0x1e, 0x36, 0x7e, 0x06, 0x06, 0x00}, // 4
    {0x7e, 0x60, 0x7c, 0x06, 0x06, 0x66, 0x3c, 0x00}, // 5
    {0x1c, 0x30, 0x60, 0x7c, 0x66, 0x66, 0x3c, 0x00}, // 6
    {0x7e, 0x66, 0x06, 0x0c, 0x18, 0x18, 0x18, 0x00}, // 7
    {0x3c, 0x66, 0x66, 0x3c, 0x66, 0x66, 0x3c, 0x00}, // 8
    {0x3c, 0x66, 0x66, 0x3e, 0x06, 0x0c, 0x38, 0x00}, // 9
    // : ; < = > ? @
    {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00}, // :
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x08, 0x10}, // ;
    {0x0c, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0c, 0x00}, // <
    {0x00, 0x7e, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00}, // =
    {0x30, 0x18, 0x0c, 0x06, 0x0c, 0x18, 0x30, 0x00}, // >
    {0x3c, 0x66, 0x06, 0x0c, 0x18, 0x00, 0x18, 0x00}, // ?
    {0x3c, 0x66, 0x6e, 0x6e, 0x60, 0x62, 0x3c, 0x00}, // @
    // A - Z
    {0x18, 0x3c, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x00}, // A
    {0x7c, 0x66, 0x66, 0x7c, 0x66, 0x66, 0x7c, 0x00}, // B
    {0x3e, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3e, 0x00}, // C
    {0x78, 0x6c, 0x66, 0x66, 0x66, 0x6c, 0x78, 0x00}, // D
    {0x7e, 0x60, 0x60, 0x78, 0x60, 0x60, 0x7e, 0x00}, // E
    {0x7e, 0x60, 0x60, 0x78, 0x60, 0x60, 0x60, 0x00}, // F
    {0x3c, 0x66, 0x60, 0x6e, 0x66, 0x66, 0x3e, 0x00}, // G
    {0x66, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x66, 0x00}, // H
    {0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00}, // I
    {0x06, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3c, 0x00}, // J
    {0x66, 0x6c, 0x78, 0x70, 0x78, 0x6c, 0x66, 0x00}, // K
    {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7e, 0x00}, // L
    {0x63, 0x77, 0x7f, 0x6b, 0x63, 0x63, 0x63, 0x00}, // M
    {0x66, 0x66, 0x76, 0x7e, 0x6e, 0x66, 0x66, 0x00}, // N
    {0x3c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00}, // O
    {0x7c, 0x66, 0x66, 0x7c, 0x60, 0x60, 0x60, 0x00}, // P
    {0x3c, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x0e, 0x00}, // Q
    {0x7c, 0x66, 0x66, 0x7c, 0x78, 0x6c, 0x66, 0x00}, // R
    {0x3e, 0x66, 0x60, 0x3c, 0x06, 0x66, 0x3c, 0x00}, // S
    {0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00}, // T
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00}, // U
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x00}, // V
    {0x63, 0x63, 0x63, 0x6b, 0x7f, 0x77, 0x63, 0x00}, // W
    {0x66, 0x66, 0x3c, 0x18, 0x3c, 0x66, 0x66, 0x00}, // X
    {0x66, 0x66, 0x66, 0x3c, 0x18, 0x18, 0x18, 0x00}, // Y
    {0x7e, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x7e, 0x00}, // Z
    // [ \ ] ^ _ `
    {0x3c, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3c, 0x00}, // [
    {0x00, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x03, 0x00}, // Backslash
    {0x3c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3c, 0x00}, // ]
    {0x08, 0x1c, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00}, // ^
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00}, // _
    {0x18, 0x18, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00}, // `
    // a - z
    {0x00, 0x00, 0x3c, 0x06, 0x3e, 0x66, 0x3b, 0x00}, // a
    {0x60, 0x60, 0x7c, 0x66, 0x66, 0x66, 0x7c, 0x00}, // b
    {0x00, 0x00, 0x3c, 0x66, 0x60, 0x66, 0x3c, 0x00}, // c
    {0x06, 0x06, 0x3e, 0x66, 0x66, 0x66, 0x3e, 0x00}, // d
    {0x00, 0x00, 0x3c, 0x66, 0x7e, 0x60, 0x3c, 0x00}, // e
    {0x1c, 0x30, 0x7c, 0x30, 0x30, 0x30, 0x30, 0x00}, // f
    {0x00, 0x00, 0x3e, 0x66, 0x66, 0x3e, 0x06, 0x3c}, // g
    {0x60, 0x60, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x00}, // h
    {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3c, 0x00}, // i
    {0x06, 0x00, 0x0e, 0x06, 0x06, 0x06, 0x06, 0x3c}, // j
    {0x60, 0x60, 0x66, 0x6c, 0x78, 0x6c, 0x66, 0x00}, // k
    {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00}, // l
    {0x00, 0x00, 0x6e, 0x7f, 0x6b, 0x63, 0x63, 0x00}, // m
    {0x00, 0x00, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x00}, // n
    {0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x3c, 0x00}, // o
    {0x00, 0x00, 0x7c, 0x66, 0x66, 0x7c, 0x60, 0x60}, // p
    {0x00, 0x00, 0x3e, 0x66, 0x66, 0x3e, 0x06, 0x06}, // q
    {0x00, 0x00, 0x7c, 0x66, 0x60, 0x60, 0x60, 0x00}, // r
    {0x00, 0x00, 0x3e, 0x60, 0x3c, 0x06, 0x7c, 0x00}, // s
    {0x18, 0x18, 0x7e, 0x18, 0x18, 0x18, 0x0d, 0x06}, // t
    {0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3b, 0x00}, // u
    {0x00, 0x00, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x00}, // v
    {0x00, 0x00, 0x63, 0x6b, 0x7f, 0x36, 0x22, 0x00}, // w
    {0x00, 0x00, 0x66, 0x3c, 0x18, 0x3c, 0x66, 0x00}, // x
    {0x00, 0x00, 0x66, 0x66, 0x66, 0x3e, 0x06, 0x3c}, // y
    {0x00, 0x00, 0x7e, 0x0c, 0x18, 0x30, 0x7e, 0x00}, // z
    // { | } ~ del
    {0x0e, 0x18, 0x18, 0x70, 0x18, 0x18, 0x0e, 0x00}, // {
    {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00}, // |
    {0x70, 0x18, 0x18, 0x0e, 0x18, 0x18, 0x70, 0x00}, // }
    {0x76, 0xdc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // ~
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}  // DEL
};

static constexpr uint32_t kColorBackground = 0xFF050505;
static constexpr uint32_t kColorPanel = 0xFF101820;
static constexpr uint32_t kColorText = 0xFFE8E8E8;
static constexpr uint32_t kColorInfo = 0xFF4CC9F0;
static constexpr uint32_t kColorWarn = 0xFFFFC857;
static constexpr uint32_t kColorError = 0xFFFF4D4D;
static constexpr uint32_t kColorOk = 0xFF00D084;

static void KernelHalt() {
    while (true) {
        asm volatile("cli; hlt");
    }
}

class KernelConsole {
public:
    bool Init(const FramebufferInfo* framebuffer) {
        if (!framebuffer || framebuffer->base_address == 0 || framebuffer->width == 0 || framebuffer->height == 0) {
            return false;
        }

        m_Framebuffer = framebuffer;
        m_CursorX = kMarginX;
        m_CursorY = kMarginY;
        Clear(kColorBackground);
        return true;
    }

    bool IsReady() const {
        return m_Framebuffer != nullptr;
    }

    void Clear(uint32_t color) {
        FillRect(0, 0, m_Framebuffer->width, m_Framebuffer->height, color);
        m_CursorX = kMarginX;
        m_CursorY = kMarginY;
    }

    void PutChar(char c, uint32_t color = kColorText) {
        if (!m_Framebuffer) {
            return;
        }

        if (c == '\n') {
            NewLine();
            return;
        }

        if (m_CursorX + kGlyphWidth > m_Framebuffer->width - kMarginX) {
            NewLine();
        }
        if (m_CursorY + kGlyphHeight > m_Framebuffer->height - kMarginY) {
            Scroll();
        }

        DrawChar(m_CursorX, m_CursorY, c, color);
        m_CursorX += kAdvanceX;
    }

    void Write(const char* text, uint32_t color = kColorText) {
        if (!text) {
            return;
        }

        while (*text) {
            PutChar(*text, color);
            text++;
        }
    }

    void WriteUnsigned(uint64_t value, uint32_t color = kColorText) {
        char buffer[32];
        int index = 0;

        if (value == 0) {
            buffer[index++] = '0';
        } else {
            while (value > 0 && index < 31) {
                buffer[index++] = static_cast<char>('0' + (value % 10));
                value /= 10;
            }
        }

        for (int i = index - 1; i >= 0; --i) {
            PutChar(buffer[i], color);
        }
    }

    void WriteHex(uint64_t value, uint32_t color = kColorText) {
        Write("0x", color);

        bool started = false;
        for (int shift = 60; shift >= 0; shift -= 4) {
            uint8_t nibble = static_cast<uint8_t>((value >> shift) & 0xF);
            if (nibble != 0 || started || shift == 0) {
                started = true;
                PutChar(static_cast<char>(nibble < 10 ? '0' + nibble : 'A' + (nibble - 10)), color);
            }
        }
    }

    void FillRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
        if (!m_Framebuffer) {
            return;
        }

        for (uint32_t cy = y; cy < y + h && cy < m_Framebuffer->height; cy++) {
            for (uint32_t cx = x; cx < x + w && cx < m_Framebuffer->width; cx++) {
                DrawPixel(cx, cy, color);
            }
        }
    }

private:
    static constexpr uint32_t kMarginX = 32;
    static constexpr uint32_t kMarginY = 28;
    static constexpr uint32_t kGlyphWidth = 16;
    static constexpr uint32_t kGlyphHeight = 16;
    static constexpr uint32_t kAdvanceX = 18;
    static constexpr uint32_t kAdvanceY = 22;

    void DrawPixel(uint32_t x, uint32_t y, uint32_t color) {
        if (x >= m_Framebuffer->width || y >= m_Framebuffer->height) {
            return;
        }

        uint32_t* base = reinterpret_cast<uint32_t*>(m_Framebuffer->base_address);
        base[y * m_Framebuffer->pixels_per_scanline + x] = color;
    }

    void DrawChar(uint32_t x, uint32_t y, char c, uint32_t color) {
        uint8_t code = static_cast<uint8_t>(c);
        if (code >= 128) {
            code = '?';
        }

        const uint8_t* glyph = font8x8_kernel[code];
        for (int gy = 0; gy < 8; gy++) {
            uint8_t row = glyph[gy];
            for (int gx = 0; gx < 8; gx++) {
                if (row & (1 << gx)) {
                    DrawPixel(x + gx * 2, y + gy * 2, color);
                    DrawPixel(x + gx * 2 + 1, y + gy * 2, color);
                    DrawPixel(x + gx * 2, y + gy * 2 + 1, color);
                    DrawPixel(x + gx * 2 + 1, y + gy * 2 + 1, color);
                }
            }
        }
    }

    void NewLine() {
        m_CursorX = kMarginX;
        m_CursorY += kAdvanceY;
    }

    void Scroll() {
        Clear(kColorBackground);
        Write("[INFO] Console scroll reset\n", kColorInfo);
    }

    const FramebufferInfo* m_Framebuffer = nullptr;
    uint32_t m_CursorX = kMarginX;
    uint32_t m_CursorY = kMarginY;
};

static KernelConsole g_Console;

static uint32_t LogColor(LogLevel level) {
    switch (level) {
        case LogLevel::Info:
            return kColorInfo;
        case LogLevel::Warn:
            return kColorWarn;
        case LogLevel::Error:
        case LogLevel::Panic:
            return kColorError;
    }

    return kColorText;
}

static const char* LogPrefix(LogLevel level) {
    switch (level) {
        case LogLevel::Info:
            return "[INFO] ";
        case LogLevel::Warn:
            return "[WARN] ";
        case LogLevel::Error:
            return "[ERROR] ";
        case LogLevel::Panic:
            return "[PANIC] ";
    }

    return "[LOG] ";
}

void KernelLog(LogLevel level, const char* message) {
    g_Console.Write(LogPrefix(level), LogColor(level));
    g_Console.Write(message, kColorText);
    g_Console.PutChar('\n');
}

void KernelPanic(const char* reason) {
    if (g_Console.IsReady()) {
        g_Console.FillRect(0, 0, 800, 120, 0xFF330000);
        KernelLog(LogLevel::Panic, reason);
        g_Console.Write("System halted.\n", kColorError);
    }

    KernelHalt();
}

struct KernelMemoryDescriptor {
    uint32_t type;
    uint32_t padding;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
};

static constexpr uint32_t kEfiConventionalMemory = 7;
static constexpr uint64_t kPageSize = 4096;
static constexpr uint64_t kManagedMemoryLimit = 0x100000000ULL;
static constexpr uint64_t kManagedPageCount = kManagedMemoryLimit / kPageSize;
static constexpr uint64_t kHeapInitialPages = 16;

static uint8_t g_PhysicalPageBitmap[kManagedPageCount / 8];

alignas(4096) static uint64_t g_Pml4[512];
alignas(4096) static uint64_t g_Pdpt[512];
alignas(4096) static uint64_t g_PageDirectories[4][512];

class PhysicalMemoryManager {
public:
    bool Init(const BootInfo& boot_info) {
        if (!boot_info.memory.buffer || boot_info.memory.descriptor_size < sizeof(KernelMemoryDescriptor)) {
            return false;
        }

        for (uint64_t i = 0; i < sizeof(g_PhysicalPageBitmap); i++) {
            g_PhysicalPageBitmap[i] = 0xFF;
        }

        m_TotalPages = 0;
        m_FreePages = 0;
        m_UsedPages = kManagedPageCount;

        const uint64_t entry_count = boot_info.memory.map_size / boot_info.memory.descriptor_size;
        const uint8_t* map = reinterpret_cast<const uint8_t*>(boot_info.memory.buffer);

        for (uint64_t i = 0; i < entry_count; i++) {
            const KernelMemoryDescriptor* desc = reinterpret_cast<const KernelMemoryDescriptor*>(map + i * boot_info.memory.descriptor_size);
            const uint64_t start = AlignUp(desc->physical_start, kPageSize);
            const uint64_t end = AlignDown(desc->physical_start + desc->number_of_pages * kPageSize, kPageSize);

            if (end <= start || start >= kManagedMemoryLimit) {
                continue;
            }

            const uint64_t clamped_end = end > kManagedMemoryLimit ? kManagedMemoryLimit : end;
            const uint64_t page_count = (clamped_end - start) / kPageSize;
            m_TotalPages += page_count;

            if (desc->type == kEfiConventionalMemory) {
                MarkRange(start, page_count, true);
            }
        }

        ReserveRange(0, 0x100000);
        ReserveRange(boot_info.kernel_base, boot_info.kernel_size);
        ReserveRange(boot_info.framebuffer.base_address,
            static_cast<uint64_t>(boot_info.framebuffer.height) * boot_info.framebuffer.pixels_per_scanline * 4);
        ReserveRange(reinterpret_cast<uint64_t>(g_PhysicalPageBitmap), sizeof(g_PhysicalPageBitmap));
        ReserveRange(reinterpret_cast<uint64_t>(g_Pml4), sizeof(g_Pml4));
        ReserveRange(reinterpret_cast<uint64_t>(g_Pdpt), sizeof(g_Pdpt));
        ReserveRange(reinterpret_cast<uint64_t>(g_PageDirectories), sizeof(g_PageDirectories));

        return m_FreePages > 0;
    }

    uint64_t AllocatePage() {
        return AllocateContiguous(1);
    }

    uint64_t AllocateContiguous(uint64_t page_count) {
        if (page_count == 0 || page_count > m_FreePages) {
            return 0;
        }

        uint64_t run_start = 0;
        uint64_t run_length = 0;

        for (uint64_t page = 0; page < kManagedPageCount; page++) {
            if (IsPageFree(page)) {
                if (run_length == 0) {
                    run_start = page;
                }
                run_length++;

                if (run_length == page_count) {
                    MarkPages(run_start, page_count, false);
                    return run_start * kPageSize;
                }
            } else {
                run_length = 0;
            }
        }

        return 0;
    }

    void FreePage(uint64_t address) {
        MarkRange(address, 1, true);
    }

    uint64_t TotalPages() const {
        return m_TotalPages;
    }

    uint64_t FreePages() const {
        return m_FreePages;
    }

    uint64_t UsedPages() const {
        return m_UsedPages;
    }

private:
    static uint64_t AlignDown(uint64_t value, uint64_t alignment) {
        return value & ~(alignment - 1);
    }

    static uint64_t AlignUp(uint64_t value, uint64_t alignment) {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    bool IsPageFree(uint64_t page) const {
        return (g_PhysicalPageBitmap[page / 8] & (1u << (page % 8))) == 0;
    }

    void SetPage(uint64_t page, bool free) {
        const uint8_t mask = static_cast<uint8_t>(1u << (page % 8));
        const bool was_free = IsPageFree(page);

        if (free) {
            g_PhysicalPageBitmap[page / 8] &= static_cast<uint8_t>(~mask);
        } else {
            g_PhysicalPageBitmap[page / 8] |= mask;
        }

        if (free && !was_free) {
            m_FreePages++;
            if (m_UsedPages > 0) {
                m_UsedPages--;
            }
        } else if (!free && was_free) {
            if (m_FreePages > 0) {
                m_FreePages--;
            }
            m_UsedPages++;
        }
    }

    void MarkPages(uint64_t start_page, uint64_t page_count, bool free) {
        for (uint64_t page = start_page; page < start_page + page_count && page < kManagedPageCount; page++) {
            SetPage(page, free);
        }
    }

    void MarkRange(uint64_t address, uint64_t page_count, bool free) {
        const uint64_t start_page = address / kPageSize;
        MarkPages(start_page, page_count, free);
    }

    void ReserveRange(uint64_t address, uint64_t byte_count) {
        if (byte_count == 0 || address >= kManagedMemoryLimit) {
            return;
        }

        const uint64_t start = AlignDown(address, kPageSize);
        const uint64_t end = AlignUp(address + byte_count, kPageSize);
        MarkRange(start, (end - start) / kPageSize, false);
    }

    uint64_t m_TotalPages = 0;
    uint64_t m_FreePages = 0;
    uint64_t m_UsedPages = 0;
};

class VirtualMemoryManager {
public:
    void InitIdentityMap4GiB() {
        for (uint64_t i = 0; i < 512; i++) {
            g_Pml4[i] = 0;
            g_Pdpt[i] = 0;
        }

        for (uint64_t pd = 0; pd < 4; pd++) {
            g_Pdpt[pd] = reinterpret_cast<uint64_t>(&g_PageDirectories[pd][0]) | kPresentWritable;
            for (uint64_t entry = 0; entry < 512; entry++) {
                const uint64_t physical = (pd * 512 + entry) * kLargePageSize;
                g_PageDirectories[pd][entry] = physical | kPresentWritable | kPageSize2MiB;
            }
        }

        g_Pml4[0] = reinterpret_cast<uint64_t>(g_Pdpt) | kPresentWritable;

        asm volatile("mov %0, %%cr3" :: "r"(reinterpret_cast<uint64_t>(g_Pml4)) : "memory");
    }

private:
    static constexpr uint64_t kPresentWritable = 0x003;
    static constexpr uint64_t kPageSize2MiB = 0x080;
    static constexpr uint64_t kLargePageSize = 2 * 1024 * 1024;
};

class KernelHeap {
public:
    bool Init(PhysicalMemoryManager& pmm) {
        const uint64_t heap_base = pmm.AllocateContiguous(kHeapInitialPages);
        if (heap_base == 0) {
            return false;
        }

        m_Start = heap_base;
        m_Current = heap_base;
        m_End = heap_base + kHeapInitialPages * kPageSize;
        return true;
    }

    void* Allocate(uint64_t size, uint64_t alignment = 16) {
        if (size == 0 || alignment == 0) {
            return nullptr;
        }

        const uint64_t aligned = (m_Current + alignment - 1) & ~(alignment - 1);
        if (aligned + size > m_End) {
            return nullptr;
        }

        m_Current = aligned + size;
        return reinterpret_cast<void*>(aligned);
    }

    uint64_t Capacity() const {
        return m_End - m_Start;
    }

    uint64_t Used() const {
        return m_Current - m_Start;
    }

private:
    uint64_t m_Start = 0;
    uint64_t m_Current = 0;
    uint64_t m_End = 0;
};

static PhysicalMemoryManager g_PhysicalMemory;
static VirtualMemoryManager g_VirtualMemory;
static KernelHeap g_KernelHeap;

void* KernelAllocate(uint64_t size, uint64_t alignment) {
    return g_KernelHeap.Allocate(size, alignment);
}

static bool KernelMemoryInit(const BootInfo& boot_info) {
    if (!g_PhysicalMemory.Init(boot_info)) {
        return false;
    }

    g_VirtualMemory.InitIdentityMap4GiB();

    if (!g_KernelHeap.Init(g_PhysicalMemory)) {
        return false;
    }

    void* probe = g_KernelHeap.Allocate(64);
    return probe != nullptr;
}

static bool KernelInit(BootInfo* boot_info) {
    if (!boot_info) {
        return false;
    }

    if (!g_Console.Init(&boot_info->framebuffer)) {
        return false;
    }

    KernelLog(LogLevel::Info, "Phase 3 kernel initialized");
    KernelLog(LogLevel::Info, "Framebuffer console online");
    return true;
}

static void PrintMemoryManagerInfo() {
    g_Console.Write("PMM: total=");
    g_Console.WriteUnsigned(g_PhysicalMemory.TotalPages());
    g_Console.Write(" pages free=");
    g_Console.WriteUnsigned(g_PhysicalMemory.FreePages());
    g_Console.Write(" used=");
    g_Console.WriteUnsigned(g_PhysicalMemory.UsedPages());
    g_Console.PutChar('\n');

    g_Console.Write("Paging: identity map 0-4GiB active\n", kColorOk);

    g_Console.Write("Kernel heap: capacity=");
    g_Console.WriteUnsigned(g_KernelHeap.Capacity());
    g_Console.Write(" used=");
    g_Console.WriteUnsigned(g_KernelHeap.Used());
    g_Console.Write(" bytes\n");
}

static void PrintBootInfo(const BootInfo& boot_info) {
    g_Console.FillRect(20, 18, 760, 120, kColorPanel);
    g_Console.Write("Phase 3 - Kernel\n", kColorOk);
    g_Console.Write("Framebuffer: ");
    g_Console.WriteUnsigned(boot_info.framebuffer.width);
    g_Console.Write("x");
    g_Console.WriteUnsigned(boot_info.framebuffer.height);
    g_Console.Write(" stride=");
    g_Console.WriteUnsigned(boot_info.framebuffer.pixels_per_scanline);
    g_Console.PutChar('\n');

    g_Console.Write("CPU: ");
    g_Console.Write(boot_info.cpu.vendor);
    g_Console.Write(" Fam=");
    g_Console.WriteUnsigned(boot_info.cpu.family);
    g_Console.Write(" Mod=");
    g_Console.WriteUnsigned(boot_info.cpu.model);
    g_Console.Write(" Step=");
    g_Console.WriteUnsigned(boot_info.cpu.stepping);
    g_Console.PutChar('\n');

    g_Console.Write("ACPI RSDP: ", kColorText);
    g_Console.Write(boot_info.rsdp ? "present" : "missing", boot_info.rsdp ? kColorOk : kColorWarn);
    g_Console.PutChar('\n');

    g_Console.Write("Memory map: buffer=");
    g_Console.WriteHex(reinterpret_cast<uint64_t>(boot_info.memory.buffer));
    g_Console.Write(" entries=");
    g_Console.WriteUnsigned(boot_info.memory.map_size / (boot_info.memory.descriptor_size ? boot_info.memory.descriptor_size : 1));
    g_Console.Write(" descriptor=");
    g_Console.WriteUnsigned(boot_info.memory.descriptor_size);
    g_Console.PutChar('\n');

    g_Console.Write("Kernel image: base=");
    g_Console.WriteHex(boot_info.kernel_base);
    g_Console.Write(" size=");
    g_Console.WriteUnsigned(boot_info.kernel_size);
    g_Console.Write(" bytes\n");
}

extern "C" void kernel_main(BootInfo* boot_info) {
    if (!KernelInit(boot_info)) {
        KernelHalt();
    }

    if (boot_info->memory.descriptor_size == 0) {
        KernelPanic("Invalid UEFI memory map descriptor size");
    }

    PrintBootInfo(*boot_info);
    KernelLog(LogLevel::Info, "Bootloader handoff data accepted");
    KernelLog(LogLevel::Info, "Logging system online");
    KernelLog(LogLevel::Info, "Panic handler armed");

    if (!KernelCpuInit()) {
        KernelPanic("CPU initialization failed");
    }

    KernelLog(LogLevel::Info, "Phase 5 CPU initialized");
    PrintCpuInfo();

    if (!KernelMemoryInit(*boot_info)) {
        KernelPanic("Kernel memory initialization failed");
    }

    KernelLog(LogLevel::Info, "Physical memory manager online");
    KernelLog(LogLevel::Info, "Virtual memory identity map installed");
    KernelLog(LogLevel::Info, "Kernel heap online");
    PrintMemoryManagerInfo();

    if (!KernelSchedulerInit()) {
        KernelPanic("Scheduler initialization failed");
    }

    KernelLog(LogLevel::Info, "Phase 6 scheduler initialized");
    KernelSchedulerRunSelfTest();
    PrintSchedulerInfo();

    while (true) {
        asm volatile("hlt");
    }
}
