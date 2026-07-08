#pragma once

#include "efi_defs.hpp"
#include "graphics_output.hpp"

class Framebuffer {
private:
    uint64_t m_BaseAddress;
    uint32_t m_Width;
    uint32_t m_Height;
    uint32_t m_PixelsPerScanLine;
    uint32_t m_PixelFormat;
    EFI_STATUS m_LastStatus;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* m_Gop;

    uint32_t ConvertColor(uint32_t color) const;

public:
    Framebuffer();
    ~Framebuffer() = default;

    // Initializes Framebuffer using Graphics Output Protocol
    bool Init();

    uint64_t GetBaseAddress() const { return m_BaseAddress; }
    uint32_t GetWidth() const { return m_Width; }
    uint32_t GetHeight() const { return m_Height; }
    uint32_t GetPixelsPerScanLine() const { return m_PixelsPerScanLine; }
    uint32_t GetPixelFormat() const { return m_PixelFormat; }
    EFI_STATUS GetLastStatus() const { return m_LastStatus; }
    const char* GetPixelFormatName() const;

    void DrawPixel(uint32_t x, uint32_t y, uint32_t color);
    void DrawRectangle(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
    void Fill(uint32_t color);
    void Clear();

    // Renders a character at (x, y) using a simple 8x8 bitmap font
    void DrawChar(uint32_t x, uint32_t y, char c, uint32_t color);

    // Renders a string starting at (x, y)
    void DrawString(uint32_t x, uint32_t y, const char* str, uint32_t color);
};
