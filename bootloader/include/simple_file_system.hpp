#pragma once

#include "efi_defs.hpp"

struct EFI_FILE_PROTOCOL;
struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

typedef EFI_STATUS (EFIAPI *EFI_VOLUME_OPEN)(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* This, EFI_FILE_PROTOCOL** Root);

struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    uint64_t Revision;
    EFI_VOLUME_OPEN OpenVolume;
};

// File protocol functions
typedef EFI_STATUS (EFIAPI *EFI_FILE_OPEN)(EFI_FILE_PROTOCOL* This, EFI_FILE_PROTOCOL** NewHandle, const char16_t* FileName, uint64_t OpenMode, uint64_t Attributes);
typedef EFI_STATUS (EFIAPI *EFI_FILE_CLOSE)(EFI_FILE_PROTOCOL* This);
typedef EFI_STATUS (EFIAPI *EFI_FILE_DELETE)(EFI_FILE_PROTOCOL* This);
typedef EFI_STATUS (EFIAPI *EFI_FILE_READ)(EFI_FILE_PROTOCOL* This, size_t* BufferSize, void* Buffer);
typedef EFI_STATUS (EFIAPI *EFI_FILE_WRITE)(EFI_FILE_PROTOCOL* This, size_t* BufferSize, const void* Buffer);
typedef EFI_STATUS (EFIAPI *EFI_FILE_GET_POSITION)(EFI_FILE_PROTOCOL* This, uint64_t* Position);
typedef EFI_STATUS (EFIAPI *EFI_FILE_SET_POSITION)(EFI_FILE_PROTOCOL* This, uint64_t Position);
typedef EFI_STATUS (EFIAPI *EFI_FILE_GET_INFO)(EFI_FILE_PROTOCOL* This, const EFI_GUID* InformationType, size_t* BufferSize, void* Buffer);
typedef EFI_STATUS (EFIAPI *EFI_FILE_SET_INFO)(EFI_FILE_PROTOCOL* This, const EFI_GUID* InformationType, size_t BufferSize, const void* Buffer);
typedef EFI_STATUS (EFIAPI *EFI_FILE_FLUSH)(EFI_FILE_PROTOCOL* This);

struct EFI_FILE_PROTOCOL {
    uint64_t Revision;
    EFI_FILE_OPEN Open;
    EFI_FILE_CLOSE Close;
    EFI_FILE_DELETE Delete;
    EFI_FILE_READ Read;
    EFI_FILE_WRITE Write;
    EFI_FILE_GET_POSITION GetPosition;
    EFI_FILE_SET_POSITION SetPosition;
    EFI_FILE_GET_INFO GetInfo;
    EFI_FILE_SET_INFO SetInfo;
    EFI_FILE_FLUSH Flush;
};

// Open modes
constexpr uint64_t EFI_FILE_MODE_READ      = 0x0000000000000001ULL;
constexpr uint64_t EFI_FILE_MODE_WRITE     = 0x0000000000000002ULL;
constexpr uint64_t EFI_FILE_MODE_CREATE    = 0x8000000000000000ULL;

// File attributes
constexpr uint64_t EFI_FILE_READ_ONLY      = 0x0000000000000001ULL;
constexpr uint64_t EFI_FILE_HIDDEN         = 0x0000000000000002ULL;
constexpr uint64_t EFI_FILE_SYSTEM         = 0x0000000000000004ULL;
constexpr uint64_t EFI_FILE_RESERVED       = 0x0000000000000008ULL;
constexpr uint64_t EFI_FILE_DIRECTORY      = 0x0000000000000010ULL;
constexpr uint64_t EFI_FILE_ARCHIVE        = 0x0000000000000020ULL;
constexpr uint64_t EFI_FILE_VALID_ATTR     = 0x0000000000000037ULL;

struct EFI_FILE_INFO {
    uint64_t Size;
    uint64_t FileSize;
    uint64_t PhysicalSize;
    EFI_TIME CreateTime;
    EFI_TIME LastAccessTime;
    EFI_TIME ModificationTime;
    uint64_t Attribute;
    char16_t FileName[1]; // Null-terminated wide char string
};
