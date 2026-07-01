#pragma once

#include "efi_defs.hpp"
#include <stdarg.h>

// UEFI Console Color Definitions
constexpr uint32_t EFI_BLACK        = 0x00;
constexpr uint32_t EFI_BLUE         = 0x01;
constexpr uint32_t EFI_GREEN        = 0x02;
constexpr uint32_t EFI_CYAN         = 0x03;
constexpr uint32_t EFI_RED          = 0x04;
constexpr uint32_t EFI_MAGENTA      = 0x05;
constexpr uint32_t EFI_BROWN        = 0x06;
constexpr uint32_t EFI_LIGHTGRAY    = 0x07;
constexpr uint32_t EFI_DARKGRAY     = 0x08;
constexpr uint32_t EFI_LIGHTBLUE    = 0x09;
constexpr uint32_t EFI_LIGHTGREEN   = 0x0A;
constexpr uint32_t EFI_LIGHTCYAN    = 0x0B;
constexpr uint32_t EFI_LIGHTRED     = 0x0C;
constexpr uint32_t EFI_LIGHTMAGENTA = 0x0D;
constexpr uint32_t EFI_YELLOW       = 0x0E;
constexpr uint32_t EFI_WHITE        = 0x0F;

constexpr uint32_t EFI_BACKGROUND_BLACK     = 0x00;
constexpr uint32_t EFI_BACKGROUND_BLUE      = 0x10;
constexpr uint32_t EFI_BACKGROUND_GREEN     = 0x20;
constexpr uint32_t EFI_BACKGROUND_CYAN      = 0x30;
constexpr uint32_t EFI_BACKGROUND_RED       = 0x40;
constexpr uint32_t EFI_BACKGROUND_MAGENTA   = 0x50;
constexpr uint32_t EFI_BACKGROUND_BROWN     = 0x60;
constexpr uint32_t EFI_BACKGROUND_LIGHTGRAY = 0x70;

class EFIConsole {
private:
    static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* s_ConOut;
    static EFI_SIMPLE_TEXT_INPUT_PROTOCOL* s_ConIn;

public:
    static void Init(EFI_SYSTEM_TABLE* systemTable);

    // Prints a raw UTF-16 string
    static void Print(const char16_t* string);

    // Prints an ASCII string (automatically converted to UTF-16 internally)
    static void Print(const char* string);

    // Standard printf-like formatting for UEFI screen
    static void PrintFormatted(const char* format, ...);
    static void PrintFormattedValist(const char* format, va_list args);

    static void ClearScreen();
    static void SetCursor(size_t col, size_t row);
    static void SetColor(uint32_t text_color, uint32_t bg_color);
    static void ResetColor();

    // Key input operations
    static bool ReadKey(EFI_KEY_DATA& key);
    static char16_t ReadChar();
};
