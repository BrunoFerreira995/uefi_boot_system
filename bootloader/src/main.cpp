#include "efi_defs.hpp"
#include "uefi_application.hpp"
#include "efi_console.hpp"
#include "file.hpp"
#include "elf_loader.hpp"
#include "memory_map.hpp"
#include "framebuffer.hpp"
#include "../../common/boot_info.hpp"

// Helper to query CPUID information
static void GetCPUInfo(CPUInfo* cpu) {
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;

    // Vendor ID
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
    *reinterpret_cast<uint32_t*>(&cpu->vendor[0]) = ebx;
    *reinterpret_cast<uint32_t*>(&cpu->vendor[4]) = edx;
    *reinterpret_cast<uint32_t*>(&cpu->vendor[8]) = ecx;
    cpu->vendor[12] = '\0';

    // Family, Model, Stepping
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    cpu->stepping = eax & 0xF;
    cpu->model = (eax >> 4) & 0xF;
    cpu->family = (eax >> 8) & 0xF;

    uint32_t ext_model = (eax >> 16) & 0xF;
    uint32_t ext_family = (eax >> 20) & 0xFF;
    if (cpu->family == 15) {
        cpu->family += ext_family;
    }
    if (cpu->family == 6 || cpu->family == 15) {
        cpu->model += (ext_model << 4);
    }
}

extern "C" EFI_STATUS EFIAPI efi_main(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE* systemTable) {
    // 1. Initialize application and console
    UEFIApplication app(imageHandle, systemTable);
    EFIConsole::Init(systemTable);

    EFIConsole::ClearScreen();
    EFIConsole::SetColor(EFI_CYAN, EFI_BACKGROUND_BLACK);
    EFIConsole::Print("==================================================\n");
    EFIConsole::Print("          Antigravity UEFI Bootloader v1.0        \n");
    EFIConsole::Print("==================================================\n\n");

    // 2. Initialize simple file system protocol on ESP
    EFIConsole::Print("Initializing File System... ");
    if (!File::InitRootVolume(imageHandle)) {
        EFIConsole::SetColor(EFI_RED, EFI_BACKGROUND_BLACK);
        EFIConsole::Print("FAILED!\n");
        app.Stall(3000000);
        return EFI_LOAD_ERROR;
    }
    EFIConsole::SetColor(EFI_GREEN, EFI_BACKGROUND_BLACK);
    EFIConsole::Print("SUCCESS\n");
    EFIConsole::SetColor(EFI_LIGHTGRAY, EFI_BACKGROUND_BLACK);
    EFIConsole::PrintFormatted("ESP root volume opened from boot device handle: %p\n",
        File::GetRootDeviceHandle());

    // List boot partition files for visual validation
    File::ListDirectory("/");

    // 3. Initialize Framebuffer GOP
    EFIConsole::Print("Initializing Graphics Output Protocol... ");
    Framebuffer fb;
    if (!fb.Init()) {
        EFIConsole::SetColor(EFI_RED, EFI_BACKGROUND_BLACK);
        EFIConsole::Print("FAILED!\n");
        EFIConsole::PrintFormatted("GOP init status = 0x%llx\n", fb.GetLastStatus());
        app.Stall(3000000);
        return EFI_UNSUPPORTED;
    }
    EFIConsole::SetColor(EFI_GREEN, EFI_BACKGROUND_BLACK);
    EFIConsole::Print("SUCCESS\n");
    EFIConsole::SetColor(EFI_LIGHTGRAY, EFI_BACKGROUND_BLACK);

    EFIConsole::PrintFormatted("GOP Resolution: %dx%d (%d pixels per scanline)\n",
        fb.GetWidth(), fb.GetHeight(), fb.GetPixelsPerScanLine());
    EFIConsole::PrintFormatted("GOP Pixel Format: %s (%d), Framebuffer=%p\n",
        fb.GetPixelFormatName(), fb.GetPixelFormat(), reinterpret_cast<void*>(fb.GetBaseAddress()));

    // GOP test visual layout: Blue background, White rectangle, Black text
    fb.Fill(0x00113366); // Smooth dark blue
    fb.DrawRectangle(150, 150, 400, 150, 0xFFFFFFFF); // White rect
    fb.DrawString(200, 200, "Boot OK - Entering Kernel...", 0xFF000000); // Black text

    // 4. Open and Load Kernel ELF64
    EFIConsole::Print("Loading /kernel/kernel.elf... ");
    File kernelFile("/kernel/kernel.elf");
    if (!kernelFile.Open()) {
        EFIConsole::SetColor(EFI_RED, EFI_BACKGROUND_BLACK);
        EFIConsole::Print("FAILED (Could not open file)!\n");
        app.Stall(5000000);
        return EFI_NOT_FOUND;
    }
    size_t kernelFileSize = kernelFile.GetSize();
    if (kernelFileSize == 0) {
        EFIConsole::SetColor(EFI_RED, EFI_BACKGROUND_BLACK);
        EFIConsole::Print("FAILED (empty or unreadable file info)!\n");
        app.Stall(5000000);
        return EFI_LOAD_ERROR;
    }
    EFIConsole::SetColor(EFI_GREEN, EFI_BACKGROUND_BLACK);
    EFIConsole::PrintFormatted("OPENED (%llu bytes from ESP)\n", kernelFileSize);
    EFIConsole::SetColor(EFI_LIGHTGRAY, EFI_BACKGROUND_BLACK);

    ElfLoader loader(kernelFile);
    if (!loader.Load()) {
        EFIConsole::SetColor(EFI_RED, EFI_BACKGROUND_BLACK);
        EFIConsole::Print("FAILED (Invalid ELF64/Error loading segments)!\n");
        app.Stall(5000000);
        return EFI_LOAD_ERROR;
    }
    EFIConsole::SetColor(EFI_GREEN, EFI_BACKGROUND_BLACK);
    EFIConsole::Print("SUCCESS\n");
    EFIConsole::SetColor(EFI_LIGHTGRAY, EFI_BACKGROUND_BLACK);

    EFIConsole::PrintFormatted("Kernel ELF Loaded: Base=%p, Size=%lld bytes, Entry=%p\n",
        reinterpret_cast<void*>(loader.GetKernelBase()), loader.GetKernelSize(), reinterpret_cast<void*>(loader.GetEntryPoint()));
    EFIConsole::Print("/kernel/kernel.elf was read through the ESP root volume opened from the boot image device.\n");

    // 5. Gather boot info metadata
    BootInfo bootInfo {};

    // Framebuffer metadata
    bootInfo.framebuffer.base_address = fb.GetBaseAddress();
    bootInfo.framebuffer.width = fb.GetWidth();
    bootInfo.framebuffer.height = fb.GetHeight();
    bootInfo.framebuffer.pixels_per_scanline = fb.GetPixelsPerScanLine();
    bootInfo.framebuffer.format = fb.GetPixelFormat();

    // Tables
    bootInfo.system_table = systemTable;
    bootInfo.runtime_services = systemTable->RuntimeServices;

    // ACPI RSDP Lookup
    bootInfo.rsdp = app.FindConfigurationTable(EFI_ACPI_20_TABLE_GUID);
    if (!bootInfo.rsdp) {
        bootInfo.rsdp = app.FindConfigurationTable(EFI_ACPI_10_TABLE_GUID);
    }
    if (bootInfo.rsdp) {
        EFIConsole::PrintFormatted("ACPI RSDP Table found at: %p\n", bootInfo.rsdp);
    } else {
        EFIConsole::Print("Warning: ACPI RSDP Table not found.\n");
    }

    // CPU details
    GetCPUInfo(&bootInfo.cpu);
    EFIConsole::PrintFormatted("CPU: %s (Fam=%d, Mod=%d, Step=%d)\n",
        bootInfo.cpu.vendor, bootInfo.cpu.family, bootInfo.cpu.model, bootInfo.cpu.stepping);

    // Boot Time
    EFI_TIME time;
    if (systemTable->RuntimeServices->GetTime(&time, nullptr) == EFI_SUCCESS) {
        bootInfo.boot_time = (uint64_t)time.Year * 10000000000ULL +
                             (uint64_t)time.Month * 100000000ULL +
                             (uint64_t)time.Day * 1000000ULL +
                             (uint64_t)time.Hour * 10000ULL +
                             (uint64_t)time.Minute * 100ULL +
                             (uint64_t)time.Second;
        EFIConsole::PrintFormatted("Boot Time: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
            time.Year, time.Month, time.Day, time.Hour, time.Minute, time.Second);
    } else {
        bootInfo.boot_time = 0;
    }

    bootInfo.kernel_base = loader.GetKernelBase();
    bootInfo.kernel_size = loader.GetKernelSize();

    // 6. Memory Map and Exit Boot Services
    kernelFile.Close();
    File::CloseRootVolume();
    EFIConsole::Print("Exiting Boot Services...\n");
    app.Stall(1000000); // 1-second delay so user can read messages

    MemoryMap memMap;
    bool exitSuccess = false;
    EFI_STATUS exitStatus = EFI_SUCCESS;

    // Retry loop in case the memory map key changes during ExitBootServices.
    // Nothing that uses Boot Services may run between MemoryMap::Get() and
    // ExitBootServices(), or the freshly returned map key can become stale.
    for (int retries = 0; retries < 5; retries++) {
        if (!memMap.Get()) {
            EFIConsole::Print("Error: Failed to obtain memory map.\n");
            app.Stall(3000000);
            return EFI_DEVICE_ERROR;
        }

        exitStatus = systemTable->BootServices->ExitBootServices(imageHandle, memMap.GetMapKey());
        if (exitStatus == EFI_SUCCESS) {
            exitSuccess = true;
            break;
        }
        // If the map key was invalidated, loop, get a fresh map, and retry.
    }

    if (!exitSuccess) {
        // Can only print if we didn't exit boot services (so status is not success)
        EFIConsole::PrintFormatted("Error: ExitBootServices failed permanently: 0x%llx\n", exitStatus);
        app.Stall(5000000);
        return EFI_DEVICE_ERROR;
    }

    // 7. Hand over to Kernel
    // Populate final memory map info in BootInfo (cannot allocate memory now!)
    bootInfo.memory.buffer = memMap.GetBuffer();
    bootInfo.memory.map_size = memMap.GetMapSize();
    bootInfo.memory.map_key = memMap.GetMapKey();
    bootInfo.memory.descriptor_size = memMap.GetDescriptorSize();
    bootInfo.memory.descriptor_version = memMap.GetDescriptorVersion();

    uint64_t entryPoint = loader.GetEntryPoint();

    // Pass BootInfo pointer in RDI (System V AMD64 ABI convention) and jump
    asm volatile (
        "mov %0, %%rdi\n\t"
        "jmp *%1\n\t"
        :
        : "r"(&bootInfo), "r"(entryPoint)
        : "rdi", "rax"
    );

    // Should never reach here
    while (true) {
        asm volatile("hlt");
    }

    return EFI_SUCCESS;
}
