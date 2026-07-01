#pragma once

#include "efi_defs.hpp"

struct EFI_LOADED_IMAGE_PROTOCOL {
    uint32_t Revision;
    EFI_HANDLE ParentHandle;
    EFI_SYSTEM_TABLE* SystemTable;
    EFI_HANDLE DeviceHandle;
    void* FilePath; // Pointer to EFI_DEVICE_PATH_PROTOCOL
    void* Reserved;
    uint32_t LoadOptionsSize;
    void* LoadOptions;
    void* ImageBase;
    uint64_t ImageSize;
    uint32_t ImageCodeType;
    uint32_t ImageDataType;
    void* Unload;
};
