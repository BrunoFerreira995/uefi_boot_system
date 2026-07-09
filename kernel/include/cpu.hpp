#pragma once

#include <stdint.h>

struct BootInfo;

bool KernelCpuInit(const BootInfo& boot_info);
void PrintCpuInfo();
