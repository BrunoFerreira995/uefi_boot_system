#pragma once

#include <stdint.h>

struct BootInfo;

using KernelIrqHandler = void (*)(uint8_t irq, void* context);

bool KernelCpuInit(const BootInfo& boot_info);
bool KernelRegisterIrqHandler(uint8_t irq, KernelIrqHandler handler, void* context);
void KernelSetIrqMask(uint8_t irq, bool masked);
void KernelEnableInterrupts();
void PrintCpuInfo();
