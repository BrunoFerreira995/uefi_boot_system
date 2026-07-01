#include "elf_loader.hpp"
#include "uefi_application.hpp"
#include "efi_console.hpp"

ElfLoader::ElfLoader(File& file)
    : m_File(file), m_KernelBase(0), m_KernelSize(0), m_EntryPoint(0) {}

bool ElfLoader::Load() {
    auto& app = UEFIApplication::Get();
    EFI_BOOT_SERVICES* bs = app.GetBootServices();

    if (!m_File.IsOpen()) {
        if (!m_File.Open()) {
            EFIConsole::PrintFormatted("Error: Failed to open ELF file.\n");
            return false;
        }
    }

    Elf64_Ehdr ehdr;
    m_File.SetPosition(0);
    if (m_File.Read(&ehdr, sizeof(Elf64_Ehdr)) != sizeof(Elf64_Ehdr)) {
        EFIConsole::PrintFormatted("Error: Failed to read ELF64 header.\n");
        return false;
    }

    // Validate ELF magic and format parameters
    if (ehdr.e_ident[0] != ELF_MAGIC[0] || ehdr.e_ident[1] != ELF_MAGIC[1] ||
        ehdr.e_ident[2] != ELF_MAGIC[2] || ehdr.e_ident[3] != ELF_MAGIC[3]) {
        EFIConsole::PrintFormatted("Error: Invalid ELF magic signature.\n");
        return false;
    }

    if (ehdr.e_ident[4] != 2) { // ELFCLASS64
        EFIConsole::PrintFormatted("Error: Not a 64-bit ELF class.\n");
        return false;
    }

    if (ehdr.e_ident[5] != 1) { // ELFDATA2LSB (little endian)
        EFIConsole::PrintFormatted("Error: ELF file is not little-endian.\n");
        return false;
    }

    if (ehdr.e_machine != EM_X86_64) {
        EFIConsole::PrintFormatted("Error: ELF machine target is not x86-64 (got %d).\n", ehdr.e_machine);
        return false;
    }

    if (ehdr.e_type != ET_EXEC && ehdr.e_type != ET_DYN) {
        EFIConsole::PrintFormatted("Error: ELF file type is not executable/shared (got %d).\n", ehdr.e_type);
        return false;
    }

    m_EntryPoint = ehdr.e_entry;

    // Load Program Headers
    size_t phdrs_size = ehdr.e_phnum * sizeof(Elf64_Phdr);
    Elf64_Phdr* phdrs = nullptr;
    EFI_STATUS status = bs->AllocatePool(EfiLoaderData, phdrs_size, reinterpret_cast<void**>(&phdrs));
    if (status != EFI_SUCCESS || !phdrs) {
        EFIConsole::PrintFormatted("Error: Failed to allocate pool for program headers (%x).\n", status);
        return false;
    }

    m_File.SetPosition(ehdr.e_phoff);
    if (m_File.Read(phdrs, phdrs_size) != phdrs_size) {
        EFIConsole::PrintFormatted("Error: Failed to read program headers.\n");
        bs->FreePool(phdrs);
        return false;
    }

    uint64_t min_addr = 0xFFFFFFFFFFFFFFFFULL;
    uint64_t max_addr = 0;

    for (size_t i = 0; i < ehdr.e_phnum; i++) {
        Elf64_Phdr& phdr = phdrs[i];
        if (phdr.p_type == PT_LOAD) {
            size_t num_pages = (phdr.p_memsz + 4095) / 4096;
            uint64_t target_addr = phdr.p_vaddr;

            EFIConsole::PrintFormatted("Loading PT_LOAD segment: VAddr=%p, PhysAddr=%p, FileSz=%d, MemSz=%d, Pages=%d\n",
                phdr.p_vaddr, phdr.p_paddr, phdr.p_filesz, phdr.p_memsz, num_pages);

            status = bs->AllocatePages(AllocateAddress, EfiLoaderData, num_pages, &target_addr);
            if (status != EFI_SUCCESS) {
                // If it fails, report it, but attempt to copy anyway (often, standard memory ranges
                // might overlap with existing identity maps, but are writable).
                EFIConsole::PrintFormatted("Warning: AllocatePages at %p returned %x. Attempting direct load...\n", target_addr, status);
            }

            // Seek and read segment data into target virtual memory address
            if (phdr.p_filesz > 0) {
                m_File.SetPosition(phdr.p_offset);
                void* dest = reinterpret_cast<void*>(phdr.p_vaddr);
                size_t read_bytes = m_File.Read(dest, phdr.p_filesz);
                if (read_bytes != phdr.p_filesz) {
                    EFIConsole::PrintFormatted("Error: Failed to read segment data (read %d of %d bytes).\n", read_bytes, phdr.p_filesz);
                    bs->FreePool(phdrs);
                    return false;
                }
            }

            // Zero out remaining memsz (BSS)
            if (phdr.p_memsz > phdr.p_filesz) {
                uint8_t* bss = reinterpret_cast<uint8_t*>(phdr.p_vaddr) + phdr.p_filesz;
                size_t bss_size = phdr.p_memsz - phdr.p_filesz;
                for (size_t b = 0; b < bss_size; b++) {
                    bss[b] = 0;
                }
            }

            if (phdr.p_vaddr < min_addr) {
                min_addr = phdr.p_vaddr;
            }
            if (phdr.p_vaddr + phdr.p_memsz > max_addr) {
                max_addr = phdr.p_vaddr + phdr.p_memsz;
            }
        }
    }

    m_KernelBase = min_addr;
    m_KernelSize = max_addr - min_addr;

    bs->FreePool(phdrs);
    return true;
}
