#include "efi_console.hpp"

EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* EFIConsole::s_ConOut = nullptr;
EFI_SIMPLE_TEXT_INPUT_PROTOCOL* EFIConsole::s_ConIn = nullptr;

void EFIConsole::Init(EFI_SYSTEM_TABLE* systemTable) {
    if (systemTable) {
        s_ConOut = systemTable->ConOut;
        s_ConIn = systemTable->ConIn;
    }
}

void EFIConsole::Print(const char16_t* string) {
    if (s_ConOut) {
        s_ConOut->OutputString(s_ConOut, string);
    }
}

void EFIConsole::Print(const char* string) {
    if (!s_ConOut || !string) return;

    // Convert ASCII to UTF-16 on the fly using a small stack buffer
    char16_t buffer[256];
    size_t i = 0;
    while (string[i] != '\0') {
        buffer[i] = static_cast<char16_t>(string[i]);
        i++;
        if (i == 255) {
            buffer[i] = L'\0';
            s_ConOut->OutputString(s_ConOut, buffer);
            i = 0;
        }
    }
    if (i > 0) {
        buffer[i] = L'\0';
        s_ConOut->OutputString(s_ConOut, buffer);
    }
}

void EFIConsole::ClearScreen() {
    if (s_ConOut) {
        s_ConOut->ClearScreen(s_ConOut);
    }
}

void EFIConsole::SetCursor(size_t col, size_t row) {
    if (s_ConOut) {
        s_ConOut->SetCursorPosition(s_ConOut, col, row);
    }
}

void EFIConsole::SetColor(uint32_t text_color, uint32_t bg_color) {
    if (s_ConOut) {
        s_ConOut->SetAttribute(s_ConOut, text_color | bg_color);
    }
}

void EFIConsole::ResetColor() {
    if (s_ConOut) {
        s_ConOut->SetAttribute(s_ConOut, EFI_WHITE | EFI_BACKGROUND_BLACK);
    }
}

bool EFIConsole::ReadKey(EFI_KEY_DATA& key) {
    if (s_ConIn) {
        EFI_STATUS status = s_ConIn->ReadKeyStroke(s_ConIn, &key);
        return status == EFI_SUCCESS;
    }
    return false;
}

char16_t EFIConsole::ReadChar() {
    EFI_KEY_DATA key;
    while (!ReadKey(key)) {
        // Wait / Poll key stroke
    }
    return key.UnicodeChar;
}

// Minimal static helper to format integers to string
static void IntToString(char16_t* buf, int64_t val, int base, bool is_signed) {
    char16_t tmp[64];
    int i = 0;
    uint64_t uval = val;
    bool negative = false;

    if (is_signed && val < 0) {
        negative = true;
        uval = -val;
    }

    if (uval == 0) {
        tmp[i++] = L'0';
    } else {
        while (uval > 0) {
            uint64_t rem = uval % base;
            tmp[i++] = (rem < 10) ? (L'0' + rem) : (L'a' + (rem - 10));
            uval /= base;
        }
    }

    if (negative) {
        *buf++ = L'-';
    }

    // Reverse string
    for (int j = i - 1; j >= 0; --j) {
        *buf++ = tmp[j];
    }
    *buf = L'\0';
}

void EFIConsole::PrintFormattedValist(const char* format, va_list args) {
    char16_t out_buf[1024];
    size_t out_idx = 0;

    auto flush_buf = [&]() {
        out_buf[out_idx] = L'\0';
        Print(out_buf);
        out_idx = 0;
    };

    for (size_t i = 0; format[i] != '\0'; ++i) {
        if (out_idx >= 1000) {
            flush_buf();
        }

        if (format[i] == '%') {
            i++;
            if (format[i] == '\0') break;

            bool is_long_long = false;
            if (format[i] == 'l') {
                i++;
                if (format[i] == 'l') {
                    is_long_long = true;
                    i++;
                }
            }

            char spec = format[i];
            if (spec == '%') {
                out_buf[out_idx++] = L'%';
            } else if (spec == 'c') {
                char c = static_cast<char>(va_arg(args, int));
                out_buf[out_idx++] = static_cast<char16_t>(c);
            } else if (spec == 'd' || spec == 'i') {
                int64_t val = is_long_long ? va_arg(args, int64_t) : va_arg(args, int);
                char16_t num_buf[64];
                IntToString(num_buf, val, 10, true);
                for (size_t n = 0; num_buf[n] != L'\0'; ++n) {
                    out_buf[out_idx++] = num_buf[n];
                }
            } else if (spec == 'u') {
                uint64_t val = is_long_long ? va_arg(args, uint64_t) : va_arg(args, unsigned int);
                char16_t num_buf[64];
                IntToString(num_buf, val, 10, false);
                for (size_t n = 0; num_buf[n] != L'\0'; ++n) {
                    out_buf[out_idx++] = num_buf[n];
                }
            } else if (spec == 'x' || spec == 'X' || spec == 'p') {
                uint64_t val;
                if (spec == 'p') {
                    val = va_arg(args, uintptr_t);
                    out_buf[out_idx++] = L'0';
                    out_buf[out_idx++] = L'x';
                } else {
                    val = is_long_long ? va_arg(args, uint64_t) : va_arg(args, unsigned int);
                }
                char16_t num_buf[64];
                IntToString(num_buf, val, 16, false);
                for (size_t n = 0; num_buf[n] != L'\0'; ++n) {
                    out_buf[out_idx++] = num_buf[n];
                }
            } else if (spec == 's') {
                const char* str = va_arg(args, const char*);
                if (!str) str = "(null)";
                for (size_t n = 0; str[n] != '\0'; ++n) {
                    if (out_idx >= 1000) flush_buf();
                    out_buf[out_idx++] = static_cast<char16_t>(str[n]);
                }
            } else if (spec == 'l' && format[i+1] == 's') {
                i++;
                const char16_t* wstr = va_arg(args, const char16_t*);
                if (!wstr) wstr = u"(null)";
                for (size_t n = 0; wstr[n] != L'\0'; ++n) {
                    if (out_idx >= 1000) flush_buf();
                    out_buf[out_idx++] = wstr[n];
                }
            } else {
                out_buf[out_idx++] = L'%';
                out_buf[out_idx++] = static_cast<char16_t>(spec);
            }
        } else {
            out_buf[out_idx++] = static_cast<char16_t>(format[i]);
        }
    }

    if (out_idx > 0) {
        flush_buf();
    }
}

void EFIConsole::PrintFormatted(const char* format, ...) {
    va_list args;
    va_start(args, format);
    PrintFormattedValist(format, args);
    va_end(args);
}
