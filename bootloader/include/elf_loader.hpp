#pragma once

#include "efi_defs.hpp"
#include "file.hpp"

// ELF64 Data Types
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;

constexpr size_t EI_NIDENT = 16;

struct Elf64_Ehdr {
    unsigned char e_ident[EI_NIDENT];
    Elf64_Half    e_type;
    Elf64_Half    e_machine;
    Elf64_Word    e_version;
    Elf64_Addr    e_entry;
    Elf64_Off     e_phoff;
    Elf64_Off     e_shoff;
    Elf64_Word    e_flags;
    Elf64_Half    e_ehsize;
    Elf64_Half    e_phentsize;
    Elf64_Half    e_phnum;
    Elf64_Half    e_shentsize;
    Elf64_Half    e_shnum;
    Elf64_Half    e_shstrndx;
};

// Segment types
constexpr Elf64_Word PT_NULL    = 0;
constexpr Elf64_Word PT_LOAD    = 1;
constexpr Elf64_Word PT_DYNAMIC = 2;
constexpr Elf64_Word PT_INTERP  = 3;
constexpr Elf64_Word PT_NOTE    = 4;
constexpr Elf64_Word PT_SHLIB   = 5;
constexpr Elf64_Word PT_PHDR    = 6;

struct Elf64_Phdr {
    Elf64_Word  p_type;
    Elf64_Word  p_flags;
    Elf64_Off   p_offset;
    Elf64_Addr  p_vaddr;
    Elf64_Addr  p_paddr;
    Elf64_Xword p_filesz;
    Elf64_Xword p_memsz;
    Elf64_Xword p_align;
};

// Machine & Magic constants
constexpr unsigned char ELF_MAGIC[4] = { 0x7f, 'E', 'L', 'F' };
constexpr Elf64_Half EM_X86_64 = 62;
constexpr Elf64_Half ET_EXEC = 2;
constexpr Elf64_Half ET_DYN = 3;

class ElfLoader {
private:
    File& m_File;
    uint64_t m_KernelBase;
    uint64_t m_KernelSize;
    uint64_t m_EntryPoint;

public:
    ElfLoader(File& file);
    ~ElfLoader() = default;

    // Parses headers, validates architecture/type, allocates pages, and loads segments
    bool Load();

    uint64_t GetEntryPoint() const { return m_EntryPoint; }
    uint64_t GetKernelBase() const { return m_KernelBase; }
    uint64_t GetKernelSize() const { return m_KernelSize; }
};
