#pragma once

#include "efi_defs.hpp"

struct EFI_DEVICE_PATH_PROTOCOL {
    uint8_t Type;
    uint8_t SubType;
    uint8_t Length[2];
};
