#pragma once

#include "efi_defs.hpp"

class MemoryMap {
private:
    void* m_Buffer;
    size_t m_MapSize;
    size_t m_MapKey;
    size_t m_DescriptorSize;
    uint32_t m_DescriptorVersion;

public:
    MemoryMap();
    ~MemoryMap();

    // Queries the memory map size, allocates a pool buffer, and retrieves the map
    bool Get();

    // Frees the allocated pool buffer
    void Free();

    void* GetBuffer() const { return m_Buffer; }
    size_t GetMapSize() const { return m_MapSize; }
    size_t GetMapKey() const { return m_MapKey; }
    size_t GetDescriptorSize() const { return m_DescriptorSize; }
    uint32_t GetDescriptorVersion() const { return m_DescriptorVersion; }

    size_t GetEntryCount() const { return m_DescriptorSize ? (m_MapSize / m_DescriptorSize) : 0; }
    EFI_MEMORY_DESCRIPTOR* GetEntry(size_t index) const;

    // Helper to calculate total free (usable) conventional memory in bytes
    uint64_t GetTotalConventionalMemory() const;

    // Helper to log all memory regions to the screen
    void Print() const;
};
