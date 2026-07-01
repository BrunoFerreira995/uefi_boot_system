#pragma once

#include <stdint.h>

struct FramebufferInfo {
    uint64_t base_address;
    uint32_t width;
    uint32_t height;
    uint32_t pixels_per_scanline;
    uint32_t format;
};

struct MemoryMapInfo {
    void*    buffer;
    uint64_t map_size;
    uint64_t map_key;
    uint64_t descriptor_size;
    uint32_t descriptor_version;
};

struct CPUInfo {
    char     vendor[13];
    uint32_t family;
    uint32_t model;
    uint32_t stepping;
};

struct BootInfo {
    FramebufferInfo framebuffer;
    MemoryMapInfo   memory;
    void*           system_table;
    void*           runtime_services;
    void*           rsdp;
    uint64_t        kernel_base;
    uint64_t        kernel_size;
    uint64_t        boot_time;
    CPUInfo         cpu;
};
