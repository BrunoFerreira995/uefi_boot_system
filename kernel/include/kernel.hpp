#pragma once

#include "../../common/boot_info.hpp"

enum class LogLevel {
    Info,
    Warn,
    Error,
    Panic,
};

void KernelLog(LogLevel level, const char* message);
void KernelPanic(const char* reason);
void* KernelAllocate(uint64_t size, uint64_t alignment = 16);

extern "C" void kernel_main(BootInfo* boot_info);
