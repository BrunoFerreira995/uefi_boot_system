#pragma once

#include <stdint.h>

#include "../../common/boot_info.hpp"

struct PerformanceStatus {
    bool hardware_acceleration_ready;
    bool optimized_compositor_ready;
    bool multicore_scheduling_ready;
    bool numa_scheduling_ready;
    bool gpu_acceleration_ready;
    bool profiling_tools_ready;
    bool benchmark_suite_ready;
    bool kernel_profiler_ready;
    bool performance_counters_ready;
    uint32_t registered_counter_count;
    uint32_t benchmark_count;
    uint32_t profiler_zone_count;
    uint64_t compositor_pixels_per_frame;
    uint64_t scheduler_balance_count;
};

bool KernelPerformanceInit(const BootInfo& boot_info);
const PerformanceStatus& KernelPerformanceStatus();
uint64_t KernelPerformanceReadCounter(const char* name);
void PrintPerformanceInfo();
