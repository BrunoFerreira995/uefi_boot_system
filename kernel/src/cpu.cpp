#include "cpu.hpp"
#include "kernel.hpp"

extern "C" void CpuLoadGdt(const void* gdtr);
extern "C" void CpuLoadIdt(const void* idtr);
extern "C" void CpuLoadTss(uint16_t selector);

extern "C" void CpuExceptionStub0();
extern "C" void CpuExceptionStub1();
extern "C" void CpuExceptionStub2();
extern "C" void CpuExceptionStub3();
extern "C" void CpuExceptionStub4();
extern "C" void CpuExceptionStub5();
extern "C" void CpuExceptionStub6();
extern "C" void CpuExceptionStub7();
extern "C" void CpuExceptionStub8();
extern "C" void CpuExceptionStub9();
extern "C" void CpuExceptionStub10();
extern "C" void CpuExceptionStub11();
extern "C" void CpuExceptionStub12();
extern "C" void CpuExceptionStub13();
extern "C" void CpuExceptionStub14();
extern "C" void CpuExceptionStub15();
extern "C" void CpuExceptionStub16();
extern "C" void CpuExceptionStub17();
extern "C" void CpuExceptionStub18();
extern "C" void CpuExceptionStub19();
extern "C" void CpuExceptionStub20();
extern "C" void CpuExceptionStub21();
extern "C" void CpuExceptionStub22();
extern "C" void CpuExceptionStub23();
extern "C" void CpuExceptionStub24();
extern "C" void CpuExceptionStub25();
extern "C" void CpuExceptionStub26();
extern "C" void CpuExceptionStub27();
extern "C" void CpuExceptionStub28();
extern "C" void CpuExceptionStub29();
extern "C" void CpuExceptionStub30();
extern "C" void CpuExceptionStub31();

namespace {

static constexpr uint16_t kKernelCodeSelector = 0x08;
static constexpr uint16_t kTssSelector = 0x18;
static constexpr uint8_t kIdtInterruptGate = 0x8E;

struct DescriptorTablePointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct Tss64 {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t io_map_base;
} __attribute__((packed));

struct IdtEntry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attributes;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct ExceptionFrame {
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

alignas(16) uint64_t g_Gdt[5];
alignas(16) Tss64 g_Tss;
alignas(16) IdtEntry g_Idt[256];
alignas(16) uint8_t g_ExceptionStack[16384];

const char* g_ExceptionNames[32] = {
    "Divide error",
    "Debug",
    "Non-maskable interrupt",
    "Breakpoint",
    "Overflow",
    "BOUND range exceeded",
    "Invalid opcode",
    "Device not available",
    "Double fault",
    "Coprocessor segment overrun",
    "Invalid TSS",
    "Segment not present",
    "Stack segment fault",
    "General protection fault",
    "Page fault",
    "Reserved exception",
    "x87 floating-point exception",
    "Alignment check",
    "Machine check",
    "SIMD floating-point exception",
    "Virtualization exception",
    "Control protection exception",
    "Reserved exception",
    "Reserved exception",
    "Reserved exception",
    "Reserved exception",
    "Reserved exception",
    "Reserved exception",
    "Hypervisor injection exception",
    "VMM communication exception",
    "Security exception",
    "Reserved exception",
};

void* g_ExceptionStubs[32] = {
    reinterpret_cast<void*>(CpuExceptionStub0),
    reinterpret_cast<void*>(CpuExceptionStub1),
    reinterpret_cast<void*>(CpuExceptionStub2),
    reinterpret_cast<void*>(CpuExceptionStub3),
    reinterpret_cast<void*>(CpuExceptionStub4),
    reinterpret_cast<void*>(CpuExceptionStub5),
    reinterpret_cast<void*>(CpuExceptionStub6),
    reinterpret_cast<void*>(CpuExceptionStub7),
    reinterpret_cast<void*>(CpuExceptionStub8),
    reinterpret_cast<void*>(CpuExceptionStub9),
    reinterpret_cast<void*>(CpuExceptionStub10),
    reinterpret_cast<void*>(CpuExceptionStub11),
    reinterpret_cast<void*>(CpuExceptionStub12),
    reinterpret_cast<void*>(CpuExceptionStub13),
    reinterpret_cast<void*>(CpuExceptionStub14),
    reinterpret_cast<void*>(CpuExceptionStub15),
    reinterpret_cast<void*>(CpuExceptionStub16),
    reinterpret_cast<void*>(CpuExceptionStub17),
    reinterpret_cast<void*>(CpuExceptionStub18),
    reinterpret_cast<void*>(CpuExceptionStub19),
    reinterpret_cast<void*>(CpuExceptionStub20),
    reinterpret_cast<void*>(CpuExceptionStub21),
    reinterpret_cast<void*>(CpuExceptionStub22),
    reinterpret_cast<void*>(CpuExceptionStub23),
    reinterpret_cast<void*>(CpuExceptionStub24),
    reinterpret_cast<void*>(CpuExceptionStub25),
    reinterpret_cast<void*>(CpuExceptionStub26),
    reinterpret_cast<void*>(CpuExceptionStub27),
    reinterpret_cast<void*>(CpuExceptionStub28),
    reinterpret_cast<void*>(CpuExceptionStub29),
    reinterpret_cast<void*>(CpuExceptionStub30),
    reinterpret_cast<void*>(CpuExceptionStub31),
};

void ClearBytes(void* ptr, uint64_t size) {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(ptr);
    for (uint64_t i = 0; i < size; i++) {
        bytes[i] = 0;
    }
}

void SetTssDescriptor(uint32_t index, uint64_t base, uint32_t limit) {
    g_Gdt[index] =
        (static_cast<uint64_t>(limit & 0xFFFF)) |
        ((base & 0xFFFFFF) << 16) |
        (static_cast<uint64_t>(0x89) << 40) |
        (static_cast<uint64_t>((limit >> 16) & 0xF) << 48) |
        ((base & 0xFF000000) << 32);
    g_Gdt[index + 1] = base >> 32;
}

void SetIdtGate(uint8_t vector, void* handler) {
    const uint64_t address = reinterpret_cast<uint64_t>(handler);
    IdtEntry& entry = g_Idt[vector];

    entry.offset_low = static_cast<uint16_t>(address & 0xFFFF);
    entry.selector = kKernelCodeSelector;
    entry.ist = 1;
    entry.type_attributes = kIdtInterruptGate;
    entry.offset_mid = static_cast<uint16_t>((address >> 16) & 0xFFFF);
    entry.offset_high = static_cast<uint32_t>((address >> 32) & 0xFFFFFFFF);
    entry.reserved = 0;
}

void InitGdt() {
    ClearBytes(g_Gdt, sizeof(g_Gdt));
    ClearBytes(&g_Tss, sizeof(g_Tss));

    g_Gdt[1] = 0x00AF9A000000FFFFULL;
    g_Gdt[2] = 0x00AF92000000FFFFULL;

    const uint64_t stack_top = reinterpret_cast<uint64_t>(g_ExceptionStack + sizeof(g_ExceptionStack));
    g_Tss.rsp0 = stack_top;
    g_Tss.ist1 = stack_top;
    g_Tss.io_map_base = sizeof(Tss64);
    SetTssDescriptor(3, reinterpret_cast<uint64_t>(&g_Tss), sizeof(Tss64) - 1);

    const DescriptorTablePointer gdtr = {
        static_cast<uint16_t>(sizeof(g_Gdt) - 1),
        reinterpret_cast<uint64_t>(g_Gdt),
    };
    CpuLoadGdt(&gdtr);
    CpuLoadTss(kTssSelector);
}

void InitIdt() {
    ClearBytes(g_Idt, sizeof(g_Idt));

    for (uint8_t vector = 0; vector < 32; vector++) {
        SetIdtGate(vector, g_ExceptionStubs[vector]);
    }

    const DescriptorTablePointer idtr = {
        static_cast<uint16_t>(sizeof(g_Idt) - 1),
        reinterpret_cast<uint64_t>(g_Idt),
    };
    CpuLoadIdt(&idtr);
}

} // namespace

extern "C" void CpuExceptionHandler(ExceptionFrame* frame) {
    KernelLog(LogLevel::Panic, "CPU exception received");

    if (frame && frame->vector < 32) {
        KernelLog(LogLevel::Panic, g_ExceptionNames[frame->vector]);
    }

    KernelPanic("Unhandled CPU exception");
}

bool KernelCpuInit() {
    InitGdt();
    InitIdt();
    KernelLog(LogLevel::Info, "GDT installed");
    KernelLog(LogLevel::Info, "TSS loaded");
    KernelLog(LogLevel::Info, "IDT exception gates installed");
    return true;
}

void PrintCpuInfo() {
    KernelLog(LogLevel::Info, "CPU exception handlers armed");
}
