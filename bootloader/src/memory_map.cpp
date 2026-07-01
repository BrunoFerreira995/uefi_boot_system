#include "memory_map.hpp"
#include "uefi_application.hpp"
#include "efi_console.hpp"

static const char* GetMemoryTypeString(uint32_t type) {
    switch (type) {
        case EfiReservedMemoryType:      return "Reserved";
        case EfiLoaderCode:              return "LoaderCode";
        case EfiLoaderData:              return "LoaderData";
        case EfiBootServicesCode:        return "BootServicesCode";
        case EfiBootServicesData:        return "BootServicesData";
        case EfiRuntimeServicesCode:     return "RuntimeServicesCode";
        case EfiRuntimeServicesData:     return "RuntimeServicesData";
        case EfiConventionalMemory:      return "Conventional";
        case EfiUnusableMemory:          return "Unusable";
        case EfiACPIReclaimMemory:       return "ACPIReclaim";
        case EfiACPIMemoryNVS:           return "ACPIMemoryNVS";
        case EfiMemoryMappedIO:          return "MMIO";
        case EfiMemoryMappedIOPortSpace: return "MMIOPort";
        case EfiPalCode:                 return "PalCode";
        case EfiPersistentMemory:        return "Persistent";
        default:                         return "Unknown";
    }
}

MemoryMap::MemoryMap()
    : m_Buffer(nullptr), m_MapSize(0), m_MapKey(0), m_DescriptorSize(0), m_DescriptorVersion(0) {}

MemoryMap::~MemoryMap() {
    Free();
}

void MemoryMap::Free() {
    if (m_Buffer) {
        auto& app = UEFIApplication::Get();
        app.GetBootServices()->FreePool(m_Buffer);
        m_Buffer = nullptr;
    }
    m_MapSize = 0;
}

bool MemoryMap::Get() {
    auto& app = UEFIApplication::Get();
    EFI_BOOT_SERVICES* bs = app.GetBootServices();

    Free();

    // Query size first
    EFI_STATUS status = bs->GetMemoryMap(&m_MapSize, nullptr, &m_MapKey, &m_DescriptorSize, &m_DescriptorVersion);
    if (status != EFI_BUFFER_TOO_SMALL) {
        return false;
    }

    // Add extra padding bytes (2048 bytes or roughly 42 descriptors)
    // as allocating pool buffer will change the memory map.
    m_MapSize += 2048;

    status = bs->AllocatePool(EfiLoaderData, m_MapSize, &m_Buffer);
    if (status != EFI_SUCCESS || !m_Buffer) {
        return false;
    }

    status = bs->GetMemoryMap(
        &m_MapSize,
        reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(m_Buffer),
        &m_MapKey,
        &m_DescriptorSize,
        &m_DescriptorVersion
    );

    if (status != EFI_SUCCESS) {
        // If it fails (e.g. because memory map grew more than buffer size), free and return false
        Free();
        return false;
    }

    return true;
}

EFI_MEMORY_DESCRIPTOR* MemoryMap::GetEntry(size_t index) const {
    if (!m_Buffer || index >= GetEntryCount()) return nullptr;
    uintptr_t base = reinterpret_cast<uintptr_t>(m_Buffer);
    return reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(base + (index * m_DescriptorSize));
}

uint64_t MemoryMap::GetTotalConventionalMemory() const {
    uint64_t total = 0;
    size_t count = GetEntryCount();
    for (size_t i = 0; i < count; i++) {
        EFI_MEMORY_DESCRIPTOR* desc = GetEntry(i);
        if (desc && desc->Type == EfiConventionalMemory) {
            total += desc->NumberOfPages * 4096;
        }
    }
    return total;
}

void MemoryMap::Print() const {
    size_t count = GetEntryCount();
    EFIConsole::PrintFormatted("Memory Map: (%d entries, DescSz=%d)\n", count, m_DescriptorSize);
    for (size_t i = 0; i < count; i++) {
        EFI_MEMORY_DESCRIPTOR* desc = GetEntry(i);
        if (!desc) continue;

        const char* typeStr = GetMemoryTypeString(desc->Type);
        EFIConsole::PrintFormatted("  [%02d] Type: %-18s Phys: %p - %p (%lld pages) Attr: %llx\n",
            static_cast<int>(i), typeStr,
            reinterpret_cast<void*>(desc->PhysicalStart),
            reinterpret_cast<void*>(desc->PhysicalStart + desc->NumberOfPages * 4096 - 1),
            desc->NumberOfPages, desc->Attribute);
    }
}
