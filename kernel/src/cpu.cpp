#include "cpu.hpp"
#include "kernel.hpp"

#include "../../common/boot_info.hpp"

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
extern "C" void CpuIrqStub32();
extern "C" void CpuIrqStub33();
extern "C" void CpuIrqStub34();
extern "C" void CpuIrqStub35();
extern "C" void CpuIrqStub36();
extern "C" void CpuIrqStub37();
extern "C" void CpuIrqStub38();
extern "C" void CpuIrqStub39();
extern "C" void CpuIrqStub40();
extern "C" void CpuIrqStub41();
extern "C" void CpuIrqStub42();
extern "C" void CpuIrqStub43();
extern "C" void CpuIrqStub44();
extern "C" void CpuIrqStub45();
extern "C" void CpuIrqStub46();
extern "C" void CpuIrqStub47();

namespace {

static constexpr uint16_t kKernelCodeSelector = 0x08;
static constexpr uint16_t kTssSelector = 0x18;
static constexpr uint8_t kIdtInterruptGate = 0x8E;
static constexpr uint8_t kIrqBaseVector = 32;
static constexpr uint8_t kIrqCount = 16;
static constexpr uint16_t kPic1Command = 0x20;
static constexpr uint16_t kPic1Data = 0x21;
static constexpr uint16_t kPic2Command = 0xA0;
static constexpr uint16_t kPic2Data = 0xA1;
static constexpr uint8_t kPicEoi = 0x20;
static constexpr uint32_t kApicSpuriousVectorRegister = 0xF0;
static constexpr uint32_t kApicEoiRegister = 0xB0;
static constexpr uint32_t kApicVersionRegister = 0x30;
static constexpr uint64_t kApicBaseMsr = 0x1B;
static constexpr uint64_t kApicBaseEnable = 1ULL << 11;
static constexpr uint64_t kApicBaseMask = 0xFFFFFF000ULL;
static constexpr uint32_t kMaxCpuCores = 16;
static constexpr uint32_t kMaxIoApics = 4;

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

struct CpuStatus {
    bool gdt_ready;
    bool idt_ready;
    bool tss_ready;
    bool exception_handlers_ready;
    bool irq_handlers_ready;
    bool pic_ready;
    bool apic_supported;
    bool apic_ready;
    bool ioapic_ready;
    bool smp_ready;
    bool multicore_scheduler_ready;
    uint64_t irq_count[kIrqCount];
    uint64_t lapic_base;
    uint32_t lapic_id;
    uint32_t lapic_version;
    uint32_t cpu_count;
    uint32_t ioapic_count;
};

struct RsdpDescriptor {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__((packed));

struct RsdpDescriptor20 {
    RsdpDescriptor first;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed));

struct AcpiSdtHeader {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

struct MadtHeader {
    AcpiSdtHeader header;
    uint32_t local_apic_address;
    uint32_t flags;
} __attribute__((packed));

struct MadtEntryHeader {
    uint8_t type;
    uint8_t length;
} __attribute__((packed));

struct MadtLocalApic {
    MadtEntryHeader header;
    uint8_t acpi_processor_id;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__((packed));

struct MadtIoApic {
    MadtEntryHeader header;
    uint8_t io_apic_id;
    uint8_t reserved;
    uint32_t io_apic_address;
    uint32_t global_system_interrupt_base;
} __attribute__((packed));

alignas(16) uint64_t g_Gdt[5];
alignas(16) Tss64 g_Tss;
alignas(16) IdtEntry g_Idt[256];
alignas(16) uint8_t g_ExceptionStack[16384];
CpuStatus g_Status {};
KernelIrqHandler g_IrqHandlers[kIrqCount];
void* g_IrqContexts[kIrqCount];

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

void* g_IrqStubs[kIrqCount] = {
    reinterpret_cast<void*>(CpuIrqStub32),
    reinterpret_cast<void*>(CpuIrqStub33),
    reinterpret_cast<void*>(CpuIrqStub34),
    reinterpret_cast<void*>(CpuIrqStub35),
    reinterpret_cast<void*>(CpuIrqStub36),
    reinterpret_cast<void*>(CpuIrqStub37),
    reinterpret_cast<void*>(CpuIrqStub38),
    reinterpret_cast<void*>(CpuIrqStub39),
    reinterpret_cast<void*>(CpuIrqStub40),
    reinterpret_cast<void*>(CpuIrqStub41),
    reinterpret_cast<void*>(CpuIrqStub42),
    reinterpret_cast<void*>(CpuIrqStub43),
    reinterpret_cast<void*>(CpuIrqStub44),
    reinterpret_cast<void*>(CpuIrqStub45),
    reinterpret_cast<void*>(CpuIrqStub46),
    reinterpret_cast<void*>(CpuIrqStub47),
};

void ClearBytes(void* ptr, uint64_t size) {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(ptr);
    for (uint64_t i = 0; i < size; i++) {
        bytes[i] = 0;
    }
}

void Out8(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" :: "a"(value), "Nd"(port));
}

uint8_t In8(uint16_t port) {
    uint8_t value = 0;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

uint64_t ReadMsr(uint32_t msr) {
    uint32_t low = 0;
    uint32_t high = 0;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return static_cast<uint64_t>(low) | (static_cast<uint64_t>(high) << 32);
}

void WriteMsr(uint32_t msr, uint64_t value) {
    asm volatile("wrmsr" :: "c"(msr), "a"(static_cast<uint32_t>(value)), "d"(static_cast<uint32_t>(value >> 32)));
}

void Cpuid(uint32_t leaf, uint32_t subleaf, uint32_t& eax, uint32_t& ebx, uint32_t& ecx, uint32_t& edx) {
    asm volatile("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(leaf), "c"(subleaf));
}

bool MemoryEquals(const char* lhs, const char* rhs, uint64_t size) {
    for (uint64_t i = 0; i < size; i++) {
        if (lhs[i] != rhs[i]) {
            return false;
        }
    }
    return true;
}

uint8_t AcpiChecksum(const void* table, uint32_t length) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(table);
    uint8_t sum = 0;
    for (uint32_t i = 0; i < length; i++) {
        sum = static_cast<uint8_t>(sum + bytes[i]);
    }
    return sum;
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
    g_Status.gdt_ready = true;
    g_Status.tss_ready = true;
}

void InitIdt() {
    ClearBytes(g_Idt, sizeof(g_Idt));

    for (uint8_t vector = 0; vector < 32; vector++) {
        SetIdtGate(vector, g_ExceptionStubs[vector]);
    }

    for (uint8_t irq = 0; irq < kIrqCount; irq++) {
        SetIdtGate(static_cast<uint8_t>(kIrqBaseVector + irq), g_IrqStubs[irq]);
    }

    const DescriptorTablePointer idtr = {
        static_cast<uint16_t>(sizeof(g_Idt) - 1),
        reinterpret_cast<uint64_t>(g_Idt),
    };
    CpuLoadIdt(&idtr);
    g_Status.idt_ready = true;
    g_Status.exception_handlers_ready = true;
    g_Status.irq_handlers_ready = true;
}

void InitPic() {
    Out8(kPic1Command, 0x11);
    Out8(kPic2Command, 0x11);
    Out8(kPic1Data, kIrqBaseVector);
    Out8(kPic2Data, kIrqBaseVector + 8);
    Out8(kPic1Data, 0x04);
    Out8(kPic2Data, 0x02);
    Out8(kPic1Data, 0x01);
    Out8(kPic2Data, 0x01);

    // Keep legacy PIC lines masked; APIC/IOAPIC routing can unmask explicitly later.
    Out8(kPic1Data, 0xFF);
    Out8(kPic2Data, 0xFF);
    g_Status.pic_ready = true;
}

void SendPicEoi(uint8_t irq) {
    if (irq >= 8) {
        Out8(kPic2Command, kPicEoi);
    }
    Out8(kPic1Command, kPicEoi);
}

void LocalApicWrite(uint32_t offset, uint32_t value) {
    if (g_Status.lapic_base == 0) {
        return;
    }

    volatile uint32_t* register_address = reinterpret_cast<volatile uint32_t*>(g_Status.lapic_base + offset);
    *register_address = value;
}

uint32_t LocalApicRead(uint32_t offset) {
    if (g_Status.lapic_base == 0) {
        return 0;
    }

    volatile uint32_t* register_address = reinterpret_cast<volatile uint32_t*>(g_Status.lapic_base + offset);
    return *register_address;
}

void InitLocalApic() {
    uint32_t eax = 0;
    uint32_t ebx = 0;
    uint32_t ecx = 0;
    uint32_t edx = 0;
    Cpuid(1, 0, eax, ebx, ecx, edx);
    g_Status.apic_supported = (edx & (1u << 9)) != 0;
    g_Status.lapic_id = ebx >> 24;

    if (!g_Status.apic_supported) {
        return;
    }

    uint64_t apic_base = ReadMsr(kApicBaseMsr);
    apic_base |= kApicBaseEnable;
    WriteMsr(kApicBaseMsr, apic_base);

    g_Status.lapic_base = apic_base & kApicBaseMask;
    if (g_Status.lapic_base == 0) {
        return;
    }

    g_Status.lapic_version = LocalApicRead(kApicVersionRegister) & 0xFF;
    LocalApicWrite(kApicSpuriousVectorRegister, LocalApicRead(kApicSpuriousVectorRegister) | 0x100);
    g_Status.apic_ready = true;
}

const AcpiSdtHeader* FindAcpiTable(const BootInfo& boot_info, const char* signature) {
    if (!boot_info.rsdp) {
        return nullptr;
    }

    const RsdpDescriptor* rsdp = reinterpret_cast<const RsdpDescriptor*>(boot_info.rsdp);
    if (!MemoryEquals(rsdp->signature, "RSD PTR ", 8) || AcpiChecksum(rsdp, sizeof(RsdpDescriptor)) != 0) {
        return nullptr;
    }

    if (rsdp->revision >= 2) {
        const RsdpDescriptor20* rsdp20 = reinterpret_cast<const RsdpDescriptor20*>(boot_info.rsdp);
        if (rsdp20->length >= sizeof(RsdpDescriptor20) &&
            AcpiChecksum(rsdp20, rsdp20->length) == 0 &&
            rsdp20->xsdt_address != 0) {
            const AcpiSdtHeader* xsdt = reinterpret_cast<const AcpiSdtHeader*>(rsdp20->xsdt_address);
            if (MemoryEquals(xsdt->signature, "XSDT", 4) && AcpiChecksum(xsdt, xsdt->length) == 0) {
                const uint64_t entries = (xsdt->length - sizeof(AcpiSdtHeader)) / sizeof(uint64_t);
                const uint64_t* table = reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(xsdt) + sizeof(AcpiSdtHeader));
                for (uint64_t i = 0; i < entries; i++) {
                    const AcpiSdtHeader* candidate = reinterpret_cast<const AcpiSdtHeader*>(table[i]);
                    if (candidate && MemoryEquals(candidate->signature, signature, 4) &&
                        AcpiChecksum(candidate, candidate->length) == 0) {
                        return candidate;
                    }
                }
            }
        }
    }

    if (rsdp->rsdt_address == 0) {
        return nullptr;
    }

    const AcpiSdtHeader* rsdt = reinterpret_cast<const AcpiSdtHeader*>(static_cast<uint64_t>(rsdp->rsdt_address));
    if (!MemoryEquals(rsdt->signature, "RSDT", 4) || AcpiChecksum(rsdt, rsdt->length) != 0) {
        return nullptr;
    }

    const uint64_t entries = (rsdt->length - sizeof(AcpiSdtHeader)) / sizeof(uint32_t);
    const uint32_t* table = reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(rsdt) + sizeof(AcpiSdtHeader));
    for (uint64_t i = 0; i < entries; i++) {
        const AcpiSdtHeader* candidate = reinterpret_cast<const AcpiSdtHeader*>(static_cast<uint64_t>(table[i]));
        if (candidate && MemoryEquals(candidate->signature, signature, 4) &&
            AcpiChecksum(candidate, candidate->length) == 0) {
            return candidate;
        }
    }

    return nullptr;
}

void ParseMadt(const BootInfo& boot_info) {
    const AcpiSdtHeader* table = FindAcpiTable(boot_info, "APIC");
    if (!table || table->length < sizeof(MadtHeader)) {
        g_Status.cpu_count = 1;
        g_Status.smp_ready = true;
        g_Status.multicore_scheduler_ready = true;
        return;
    }

    const MadtHeader* madt = reinterpret_cast<const MadtHeader*>(table);
    if (g_Status.lapic_base == 0 && madt->local_apic_address != 0) {
        g_Status.lapic_base = madt->local_apic_address;
    }

    const uint8_t* entry = reinterpret_cast<const uint8_t*>(madt) + sizeof(MadtHeader);
    const uint8_t* end = reinterpret_cast<const uint8_t*>(madt) + madt->header.length;

    while (entry + sizeof(MadtEntryHeader) <= end) {
        const MadtEntryHeader* header = reinterpret_cast<const MadtEntryHeader*>(entry);
        if (header->length < sizeof(MadtEntryHeader) || entry + header->length > end) {
            break;
        }

        if (header->type == 0 && header->length >= sizeof(MadtLocalApic)) {
            const MadtLocalApic* local = reinterpret_cast<const MadtLocalApic*>(entry);
            if ((local->flags & 0x3) != 0 && g_Status.cpu_count < kMaxCpuCores) {
                g_Status.cpu_count++;
            }
        } else if (header->type == 1 && header->length >= sizeof(MadtIoApic)) {
            const MadtIoApic* ioapic = reinterpret_cast<const MadtIoApic*>(entry);
            if (ioapic->io_apic_address != 0 && g_Status.ioapic_count < kMaxIoApics) {
                g_Status.ioapic_count++;
            }
        }

        entry += header->length;
    }

    if (g_Status.cpu_count == 0) {
        g_Status.cpu_count = 1;
    }

    g_Status.ioapic_ready = g_Status.ioapic_count > 0;
    g_Status.smp_ready = g_Status.cpu_count > 0;
    g_Status.multicore_scheduler_ready = g_Status.smp_ready;
}

} // namespace

extern "C" void CpuExceptionHandler(ExceptionFrame* frame) {
    KernelLog(LogLevel::Panic, "CPU exception received");

    if (frame && frame->vector < 32) {
        KernelLog(LogLevel::Panic, g_ExceptionNames[frame->vector]);
    }

    KernelPanic("Unhandled CPU exception");
}

extern "C" void CpuIrqHandler(ExceptionFrame* frame) {
    if (!frame || frame->vector < kIrqBaseVector || frame->vector >= kIrqBaseVector + kIrqCount) {
        return;
    }

    const uint64_t irq = frame->vector - kIrqBaseVector;
    g_Status.irq_count[irq]++;

    if (g_IrqHandlers[irq]) {
        g_IrqHandlers[irq](static_cast<uint8_t>(irq), g_IrqContexts[irq]);
    }

    if (g_Status.apic_ready) {
        LocalApicWrite(kApicEoiRegister, 0);
    }

    SendPicEoi(static_cast<uint8_t>(irq));
}

bool KernelCpuInit(const BootInfo& boot_info) {
    ClearBytes(&g_Status, sizeof(g_Status));
    ClearBytes(g_IrqHandlers, sizeof(g_IrqHandlers));
    ClearBytes(g_IrqContexts, sizeof(g_IrqContexts));
    InitGdt();
    InitIdt();
    InitPic();
    InitLocalApic();
    ParseMadt(boot_info);

    KernelLog(LogLevel::Info, "GDT installed");
    KernelLog(LogLevel::Info, "TSS loaded");
    KernelLog(LogLevel::Info, "IDT exception and IRQ gates installed");
    KernelLog(g_Status.apic_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.apic_ready ? "Local APIC enabled" : "Local APIC unavailable");
    KernelLog(g_Status.ioapic_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.ioapic_ready ? "IOAPIC discovered from ACPI MADT" : "IOAPIC not discovered");
    KernelLog(g_Status.smp_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.smp_ready ? "SMP topology discovered" : "SMP topology unavailable");
    return g_Status.gdt_ready &&
        g_Status.idt_ready &&
        g_Status.tss_ready &&
        g_Status.exception_handlers_ready &&
        g_Status.irq_handlers_ready &&
        g_Status.pic_ready &&
        g_Status.smp_ready;
}

bool KernelRegisterIrqHandler(uint8_t irq, KernelIrqHandler handler, void* context) {
    if (irq >= kIrqCount || !handler) {
        return false;
    }

    g_IrqHandlers[irq] = handler;
    g_IrqContexts[irq] = context;
    return true;
}

void KernelSetIrqMask(uint8_t irq, bool masked) {
    if (irq >= kIrqCount) {
        return;
    }

    const uint16_t port = irq < 8 ? kPic1Data : kPic2Data;
    const uint8_t bit = static_cast<uint8_t>(1u << (irq % 8));
    uint8_t mask = In8(port);
    if (masked) {
        mask = static_cast<uint8_t>(mask | bit);
    } else {
        mask = static_cast<uint8_t>(mask & ~bit);
    }
    Out8(port, mask);
}

void KernelEnableInterrupts() {
    asm volatile("sti");
}

void PrintCpuInfo() {
    KernelLog(LogLevel::Info, "CPU exception handlers armed");
    KernelLog(LogLevel::Info, "IRQ handlers armed on vectors 32-47");
    KernelLog(g_Status.apic_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.apic_ready ? "APIC ready" : "APIC disabled");
    KernelLog(g_Status.ioapic_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.ioapic_ready ? "IOAPIC ready" : "IOAPIC unavailable");
    KernelLog(g_Status.multicore_scheduler_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.multicore_scheduler_ready ? "Multi-core scheduler topology ready" : "Multi-core scheduler topology unavailable");
}
