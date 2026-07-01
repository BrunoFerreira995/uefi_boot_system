#pragma once

#include "efi_defs.hpp"

struct EFI_GRAPHICS_OUTPUT_BLT_PIXEL {
    uint8_t Blue;
    uint8_t Green;
    uint8_t Red;
    uint8_t Reserved;
};

enum EFI_GRAPHICS_PIXEL_FORMAT {
    PixelRedGreenBlueReserved8BitPerColor,
    PixelBlueGreenRedReserved8BitPerColor,
    PixelBitMask,
    PixelBltOnly,
    PixelFormatMax
};

struct EFI_PIXEL_BITMASK {
    uint32_t RedMask;
    uint32_t GreenMask;
    uint32_t BlueMask;
    uint32_t ReservedMask;
};

struct EFI_GRAPHICS_OUTPUT_MODE_INFORMATION {
    uint32_t Version;
    uint32_t HorizontalResolution;
    uint32_t VerticalResolution;
    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
    EFI_PIXEL_BITMASK PixelInformation;
    uint32_t PixelsPerScanLine;
};

struct EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE {
    uint32_t MaxMode;
    uint32_t Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* Info;
    size_t SizeOfInfo;
    uint64_t FrameBufferBase;
    size_t FrameBufferSize;
};

struct EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef EFI_STATUS (EFIAPI *EFI_GRAPHICS_OUTPUT_PROTOCOL_QUERY_MODE)(
    EFI_GRAPHICS_OUTPUT_PROTOCOL* This,
    uint32_t ModeNumber,
    size_t* SizeOfInfo,
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION** Info
);

typedef EFI_STATUS (EFIAPI *EFI_GRAPHICS_OUTPUT_PROTOCOL_SET_MODE)(
    EFI_GRAPHICS_OUTPUT_PROTOCOL* This,
    uint32_t ModeNumber
);

typedef EFI_STATUS (EFIAPI *EFI_GRAPHICS_OUTPUT_PROTOCOL_BLT)(
    EFI_GRAPHICS_OUTPUT_PROTOCOL* This,
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL* BltBuffer,
    uint32_t BltOperation,
    size_t SourceX,
    size_t SourceY,
    size_t DestinationX,
    size_t DestinationY,
    size_t Width,
    size_t Height,
    size_t Delta
);

struct EFI_GRAPHICS_OUTPUT_PROTOCOL {
    EFI_GRAPHICS_OUTPUT_PROTOCOL_QUERY_MODE QueryMode;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_SET_MODE SetMode;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_BLT Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE* Mode;
};
