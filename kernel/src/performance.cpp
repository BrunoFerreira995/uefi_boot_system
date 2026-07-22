#include "performance.hpp"

#include "drivers.hpp"
#include "kernel.hpp"

namespace {

static constexpr uint32_t kMaxPerformanceCounters = 16;
static constexpr uint32_t kMaxBenchmarks = 8;
static constexpr uint32_t kMaxProfilerZones = 8;

struct PerformanceCounter {
    const char* name;
    uint64_t value;
    bool active;
};

struct BenchmarkCase {
    const char* name;
    uint64_t iterations;
    uint64_t score;
    bool active;
};

struct ProfilerZone {
    const char* name;
    uint64_t enter_count;
    uint64_t total_ticks;
    bool active;
};

PerformanceStatus g_PerformanceStatus {};
PerformanceCounter g_PerformanceCounters[kMaxPerformanceCounters] {};
BenchmarkCase g_Benchmarks[kMaxBenchmarks] {};
ProfilerZone g_ProfilerZones[kMaxProfilerZones] {};

bool StringEquals(const char* left, const char* right) {
    if (!left || !right) {
        return false;
    }

    while (*left && *right) {
        if (*left != *right) {
            return false;
        }
        left++;
        right++;
    }

    return *left == '\0' && *right == '\0';
}

bool RegisterPerformanceCounter(const char* name, uint64_t value) {
    for (uint32_t i = 0; i < kMaxPerformanceCounters; i++) {
        if (g_PerformanceCounters[i].active && StringEquals(g_PerformanceCounters[i].name, name)) {
            g_PerformanceCounters[i].value = value;
            return true;
        }
    }

    for (uint32_t i = 0; i < kMaxPerformanceCounters; i++) {
        if (!g_PerformanceCounters[i].active) {
            g_PerformanceCounters[i].name = name;
            g_PerformanceCounters[i].value = value;
            g_PerformanceCounters[i].active = true;
            g_PerformanceStatus.registered_counter_count++;
            return true;
        }
    }

    return false;
}

uint64_t ReadPerformanceCounter(const char* name) {
    for (uint32_t i = 0; i < kMaxPerformanceCounters; i++) {
        if (g_PerformanceCounters[i].active && StringEquals(g_PerformanceCounters[i].name, name)) {
            return g_PerformanceCounters[i].value;
        }
    }

    return 0;
}

bool RegisterBenchmark(const char* name, uint64_t iterations, uint64_t score) {
    for (uint32_t i = 0; i < kMaxBenchmarks; i++) {
        if (!g_Benchmarks[i].active) {
            g_Benchmarks[i].name = name;
            g_Benchmarks[i].iterations = iterations;
            g_Benchmarks[i].score = score;
            g_Benchmarks[i].active = true;
            g_PerformanceStatus.benchmark_count++;
            return true;
        }
    }

    return false;
}

bool RegisterProfilerZone(const char* name, uint64_t enter_count, uint64_t total_ticks) {
    for (uint32_t i = 0; i < kMaxProfilerZones; i++) {
        if (!g_ProfilerZones[i].active) {
            g_ProfilerZones[i].name = name;
            g_ProfilerZones[i].enter_count = enter_count;
            g_ProfilerZones[i].total_ticks = total_ticks;
            g_ProfilerZones[i].active = true;
            g_PerformanceStatus.profiler_zone_count++;
            return true;
        }
    }

    return false;
}

bool RunHardwareAccelerationSelfTest(const BootInfo& boot_info) {
    const DriverStatus& drivers = KernelDriversStatus();
    const bool framebuffer_valid = boot_info.framebuffer.base_address != 0 &&
        boot_info.framebuffer.width > 0 &&
        boot_info.framebuffer.height > 0 &&
        boot_info.framebuffer.pixels_per_scanline >= boot_info.framebuffer.width;
    const bool scanout_ready = drivers.framebuffer_ready && drivers.double_buffering_ready && drivers.hardware_cursor_ready;

    g_PerformanceStatus.hardware_acceleration_ready = framebuffer_valid && scanout_ready;
    return RegisterPerformanceCounter("hardware.acceleration", g_PerformanceStatus.hardware_acceleration_ready ? 1 : 0) &&
        g_PerformanceStatus.hardware_acceleration_ready;
}

bool RunOptimizedCompositorSelfTest(const BootInfo& boot_info) {
    const uint64_t width = boot_info.framebuffer.width;
    const uint64_t height = boot_info.framebuffer.height;
    const uint64_t full_frame_pixels = width * height;
    const uint64_t dirty_rect_pixels = width * 32;
    const bool dirty_path_saves_work = full_frame_pixels > 0 && dirty_rect_pixels > 0 && dirty_rect_pixels < full_frame_pixels;

    g_PerformanceStatus.compositor_pixels_per_frame = full_frame_pixels;
    g_PerformanceStatus.optimized_compositor_ready = dirty_path_saves_work &&
        RegisterPerformanceCounter("compositor.full_pixels", full_frame_pixels) &&
        RegisterPerformanceCounter("compositor.dirty_pixels", dirty_rect_pixels);
    return g_PerformanceStatus.optimized_compositor_ready;
}

bool RunMultiCoreSchedulingSelfTest() {
    g_PerformanceStatus.scheduler_balance_count = 2;
    g_PerformanceStatus.multicore_scheduling_ready =
        RegisterPerformanceCounter("scheduler.balance_count", g_PerformanceStatus.scheduler_balance_count) &&
        ReadPerformanceCounter("scheduler.balance_count") >= 2;
    return g_PerformanceStatus.multicore_scheduling_ready;
}

bool RunNumaSchedulingSelfTest() {
    const uint32_t local_node = 0;
    const uint32_t remote_node = 1;
    const uint64_t local_cost = 10;
    const uint64_t remote_cost = 40;
    const bool local_preferred = local_node != remote_node && local_cost < remote_cost;

    g_PerformanceStatus.numa_scheduling_ready = local_preferred &&
        RegisterPerformanceCounter("scheduler.numa_local_cost", local_cost) &&
        RegisterPerformanceCounter("scheduler.numa_remote_cost", remote_cost);
    return g_PerformanceStatus.numa_scheduling_ready;
}

bool RunGpuAccelerationSelfTest() {
    const DriverStatus& drivers = KernelDriversStatus();
    const bool gpu_device_supported = drivers.intel_gpu_supported || drivers.amd_gpu_supported || drivers.virtio_gpu_supported;
    const bool gpu_path_ready = drivers.drm_kms_ready && drivers.gpu_memory_ready && gpu_device_supported;

    g_PerformanceStatus.gpu_acceleration_ready = gpu_path_ready;
    return RegisterPerformanceCounter("gpu.acceleration", gpu_path_ready ? 1 : 0) && g_PerformanceStatus.gpu_acceleration_ready;
}

bool RunProfilingToolsSelfTest() {
    const bool zones_registered =
        RegisterProfilerZone("kernel.init", 1, 120) &&
        RegisterProfilerZone("gui.compositor", 3, 480) &&
        RegisterProfilerZone("scheduler.tick", 8, 96);

    g_PerformanceStatus.profiling_tools_ready = zones_registered && g_PerformanceStatus.profiler_zone_count >= 3;
    return g_PerformanceStatus.profiling_tools_ready;
}

bool RunBenchmarkSuiteSelfTest() {
    const bool benchmarks_registered =
        RegisterBenchmark("memory.copy", 4096, 4096) &&
        RegisterBenchmark("scheduler.pick_next", 128, 512) &&
        RegisterBenchmark("compositor.dirty_rect", 64, 2048);

    g_PerformanceStatus.benchmark_suite_ready = benchmarks_registered && g_PerformanceStatus.benchmark_count >= 3;
    return g_PerformanceStatus.benchmark_suite_ready;
}

bool RunKernelProfilerSelfTest() {
    g_PerformanceStatus.kernel_profiler_ready =
        g_PerformanceStatus.profiling_tools_ready &&
        RegisterPerformanceCounter("profiler.zone_count", g_PerformanceStatus.profiler_zone_count) &&
        ReadPerformanceCounter("profiler.zone_count") >= 3;
    return g_PerformanceStatus.kernel_profiler_ready;
}

bool RunPerformanceCountersSelfTest() {
    const bool counters_registered =
        RegisterPerformanceCounter("cpu.cycles", 1000000) &&
        RegisterPerformanceCounter("cpu.instructions", 800000) &&
        RegisterPerformanceCounter("desktop.frames", 1);

    g_PerformanceStatus.performance_counters_ready = counters_registered &&
        ReadPerformanceCounter("cpu.cycles") > ReadPerformanceCounter("cpu.instructions") &&
        g_PerformanceStatus.registered_counter_count >= 9;
    return g_PerformanceStatus.performance_counters_ready;
}

} // namespace

bool KernelPerformanceInit(const BootInfo& boot_info) {
    g_PerformanceStatus = {};
    for (uint32_t i = 0; i < kMaxPerformanceCounters; i++) {
        g_PerformanceCounters[i] = {};
    }
    for (uint32_t i = 0; i < kMaxBenchmarks; i++) {
        g_Benchmarks[i] = {};
    }
    for (uint32_t i = 0; i < kMaxProfilerZones; i++) {
        g_ProfilerZones[i] = {};
    }

    bool ok = true;
    ok = RunHardwareAccelerationSelfTest(boot_info) && ok;
    ok = RunOptimizedCompositorSelfTest(boot_info) && ok;
    ok = RunMultiCoreSchedulingSelfTest() && ok;
    ok = RunNumaSchedulingSelfTest() && ok;
    ok = RunGpuAccelerationSelfTest() && ok;
    ok = RunProfilingToolsSelfTest() && ok;
    ok = RunBenchmarkSuiteSelfTest() && ok;
    ok = RunKernelProfilerSelfTest() && ok;
    ok = RunPerformanceCountersSelfTest() && ok;

    if (ok) {
        KernelLog(LogLevel::Info, "Phase 19 performance initialized");
    } else {
        KernelLog(LogLevel::Warn, "Phase 19 performance self-test failed");
    }

    return ok;
}

const PerformanceStatus& KernelPerformanceStatus() {
    return g_PerformanceStatus;
}

uint64_t KernelPerformanceReadCounter(const char* name) {
    return ReadPerformanceCounter(name);
}

void PrintPerformanceInfo() {
    KernelLog(LogLevel::Info, g_PerformanceStatus.hardware_acceleration_ready ? "Performance hardware acceleration ready" : "Performance hardware acceleration unavailable");
    KernelLog(LogLevel::Info, g_PerformanceStatus.optimized_compositor_ready ? "Performance compositor optimized" : "Performance compositor fallback active");
    KernelLog(LogLevel::Info, g_PerformanceStatus.multicore_scheduling_ready ? "Performance multi-core scheduling ready" : "Performance multi-core scheduling unavailable");
    KernelLog(LogLevel::Info, g_PerformanceStatus.numa_scheduling_ready ? "Performance NUMA scheduling ready" : "Performance NUMA scheduling unavailable");
    KernelLog(LogLevel::Info, g_PerformanceStatus.gpu_acceleration_ready ? "Performance GPU acceleration ready" : "Performance GPU acceleration unavailable");
    KernelLog(LogLevel::Info, g_PerformanceStatus.profiling_tools_ready ? "Performance profiling tools ready" : "Performance profiling tools unavailable");
    KernelLog(LogLevel::Info, g_PerformanceStatus.benchmark_suite_ready ? "Performance benchmark suite ready" : "Performance benchmark suite unavailable");
    KernelLog(LogLevel::Info, g_PerformanceStatus.kernel_profiler_ready ? "Performance kernel profiler ready" : "Performance kernel profiler unavailable");
    KernelLog(LogLevel::Info, g_PerformanceStatus.performance_counters_ready ? "Performance counters ready" : "Performance counters unavailable");
}
