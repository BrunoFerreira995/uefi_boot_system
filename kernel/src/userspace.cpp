#include "userspace.hpp"

#include "kernel.hpp"
#include "scheduler.hpp"

namespace {

enum class SyscallNumber : uint64_t {
    Write = 1,
    Exit = 2,
    Yield = 3,
    GetPid = 4,
    Open = 5,
    Read = 6,
    Close = 7,
};

enum class UserProcessState : uint8_t {
    Empty,
    Ready,
    Running,
    Exited,
};

struct UserProcess {
    uint64_t process_id;
    const char* name;
    UserProcessState state;
    bool ring3_intent;
};

struct Elf64Header {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t program_header_offset;
    uint64_t section_header_offset;
    uint32_t flags;
    uint16_t header_size;
    uint16_t program_header_entry_size;
    uint16_t program_header_count;
    uint16_t section_header_entry_size;
    uint16_t section_header_count;
    uint16_t section_name_index;
} __attribute__((packed));

struct Elf64ProgramHeader {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t virtual_address;
    uint64_t physical_address;
    uint64_t file_size;
    uint64_t memory_size;
    uint64_t align;
} __attribute__((packed));

struct UserElfImage {
    uint64_t entry;
    uint64_t lowest_address;
    uint64_t highest_address;
    uint16_t segment_count;
    bool valid;
};

struct DynamicSymbol {
    const char* name;
    uint64_t address;
    bool resolved;
};

struct PosixFileDescriptor {
    int32_t fd;
    const char* path;
    bool open;
};

static constexpr uint64_t kMaxUserProcesses = 8;
static constexpr uint32_t kMaxDynamicSymbols = 8;
static constexpr uint32_t kMaxPosixFileDescriptors = 8;
static constexpr uint32_t kElfLoadSegment = 1;
static constexpr uint16_t kElfTypeExecutable = 2;
static constexpr uint16_t kElfMachineX86_64 = 0x3E;
static constexpr uint32_t kElfVersionCurrent = 1;
static constexpr uint8_t kElfClass64 = 2;
static constexpr uint8_t kElfDataLittleEndian = 1;

UserspaceStatus g_Status {};
UserProcess g_UserProcesses[kMaxUserProcesses];
DynamicSymbol g_DynamicSymbols[kMaxDynamicSymbols];
PosixFileDescriptor g_PosixFileDescriptors[kMaxPosixFileDescriptors];

void ResetUserspaceStatus() {
    g_Status.syscalls_ready = false;
    g_Status.shell_ready = false;
    g_Status.user_process_ready = false;
    g_Status.elf_loader_ready = false;
    g_Status.dynamic_linker_ready = false;
    g_Status.libc_ready = false;
    g_Status.posix_ready = false;
    g_Status.shell_process_id = 0;
    g_Status.shell_thread_id = 0;
    g_Status.syscall_count = 0;
    g_Status.loaded_elf_count = 0;

    for (uint64_t i = 0; i < kMaxUserProcesses; i++) {
        g_UserProcesses[i].process_id = 0;
        g_UserProcesses[i].name = nullptr;
        g_UserProcesses[i].state = UserProcessState::Empty;
        g_UserProcesses[i].ring3_intent = false;
    }

    for (uint32_t i = 0; i < kMaxDynamicSymbols; i++) {
        g_DynamicSymbols[i].name = nullptr;
        g_DynamicSymbols[i].address = 0;
        g_DynamicSymbols[i].resolved = false;
    }

    for (uint32_t i = 0; i < kMaxPosixFileDescriptors; i++) {
        g_PosixFileDescriptors[i].fd = -1;
        g_PosixFileDescriptors[i].path = nullptr;
        g_PosixFileDescriptors[i].open = false;
    }
}

bool StringEquals(const char* a, const char* b) {
    if (!a || !b) {
        return false;
    }

    while (*a && *b) {
        if (*a != *b) {
            return false;
        }
        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

UserProcess* RegisterUserProcess(uint64_t process_id, const char* name) {
    if (process_id == 0 || !name) {
        return nullptr;
    }

    for (uint64_t i = 0; i < kMaxUserProcesses; i++) {
        if (g_UserProcesses[i].state == UserProcessState::Empty) {
            g_UserProcesses[i].process_id = process_id;
            g_UserProcesses[i].name = name;
            g_UserProcesses[i].state = UserProcessState::Ready;
            g_UserProcesses[i].ring3_intent = true;
            return &g_UserProcesses[i];
        }
    }

    return nullptr;
}

UserProcess* FindUserProcess(uint64_t process_id) {
    for (uint64_t i = 0; i < kMaxUserProcesses; i++) {
        if (g_UserProcesses[i].process_id == process_id &&
            g_UserProcesses[i].state != UserProcessState::Empty) {
            return &g_UserProcesses[i];
        }
    }

    return nullptr;
}

uint64_t SysWrite(const char* text, uint64_t length) {
    if (!text) {
        return 0;
    }

    uint64_t written = 0;
    while (written < length && text[written] != '\0') {
        char buffer[2] = {text[written], '\0'};
        KernelLog(LogLevel::Info, buffer);
        written++;
    }

    return written;
}

uint64_t SysExit(uint64_t process_id, uint64_t code) {
    UserProcess* process = FindUserProcess(process_id);
    if (!process) {
        return 0;
    }

    process->state = UserProcessState::Exited;
    static_cast<void>(code);
    return 1;
}

int32_t PosixOpen(const char* path, int32_t flags) {
    static_cast<void>(flags);
    if (!path) {
        return -1;
    }

    for (uint32_t i = 0; i < kMaxPosixFileDescriptors; i++) {
        if (!g_PosixFileDescriptors[i].open) {
            g_PosixFileDescriptors[i].fd = static_cast<int32_t>(i + 3);
            g_PosixFileDescriptors[i].path = path;
            g_PosixFileDescriptors[i].open = true;
            return g_PosixFileDescriptors[i].fd;
        }
    }

    return -1;
}

uint64_t PosixRead(int32_t fd, char* buffer, uint64_t length) {
    if (fd < 3 || !buffer || length == 0) {
        return 0;
    }

    for (uint32_t i = 0; i < kMaxPosixFileDescriptors; i++) {
        if (g_PosixFileDescriptors[i].open && g_PosixFileDescriptors[i].fd == fd) {
            buffer[0] = '\0';
            return 0;
        }
    }

    return 0;
}

int32_t PosixClose(int32_t fd) {
    for (uint32_t i = 0; i < kMaxPosixFileDescriptors; i++) {
        if (g_PosixFileDescriptors[i].open && g_PosixFileDescriptors[i].fd == fd) {
            g_PosixFileDescriptors[i].open = false;
            g_PosixFileDescriptors[i].fd = -1;
            g_PosixFileDescriptors[i].path = nullptr;
            return 0;
        }
    }

    return -1;
}

uint64_t LibcWrite(int32_t fd, const char* text) {
    if (fd != 1 && fd != 2) {
        return 0;
    }
    if (!text) {
        return 0;
    }

    uint64_t length = 0;
    while (text[length]) {
        length++;
    }

    return KernelSyscall(static_cast<uint64_t>(SyscallNumber::Write),
        reinterpret_cast<uint64_t>(text),
        length,
        0);
}

bool ValidateElf64Header(const Elf64Header& header, uint64_t image_size) {
    return image_size >= sizeof(Elf64Header) &&
        header.ident[0] == 0x7F &&
        header.ident[1] == 'E' &&
        header.ident[2] == 'L' &&
        header.ident[3] == 'F' &&
        header.ident[4] == kElfClass64 &&
        header.ident[5] == kElfDataLittleEndian &&
        header.type == kElfTypeExecutable &&
        header.machine == kElfMachineX86_64 &&
        header.version == kElfVersionCurrent &&
        header.header_size == sizeof(Elf64Header) &&
        header.program_header_entry_size == sizeof(Elf64ProgramHeader) &&
        header.program_header_count > 0 &&
        header.program_header_offset + header.program_header_count * sizeof(Elf64ProgramHeader) <= image_size;
}

bool LoadUserElfImage(const uint8_t* image, uint64_t image_size, UserElfImage& loaded) {
    loaded.entry = 0;
    loaded.lowest_address = 0;
    loaded.highest_address = 0;
    loaded.segment_count = 0;
    loaded.valid = false;

    if (!image || image_size < sizeof(Elf64Header)) {
        return false;
    }

    const Elf64Header* header = reinterpret_cast<const Elf64Header*>(image);
    if (!ValidateElf64Header(*header, image_size)) {
        return false;
    }

    loaded.entry = header->entry;
    const Elf64ProgramHeader* programs =
        reinterpret_cast<const Elf64ProgramHeader*>(image + header->program_header_offset);

    for (uint16_t i = 0; i < header->program_header_count; i++) {
        const Elf64ProgramHeader& program = programs[i];
        if (program.type != kElfLoadSegment) {
            continue;
        }
        if (program.file_size > program.memory_size || program.offset + program.file_size > image_size) {
            return false;
        }

        const uint64_t start = program.virtual_address;
        const uint64_t end = program.virtual_address + program.memory_size;
        if (end <= start) {
            return false;
        }

        if (loaded.segment_count == 0 || start < loaded.lowest_address) {
            loaded.lowest_address = start;
        }
        if (end > loaded.highest_address) {
            loaded.highest_address = end;
        }
        loaded.segment_count++;
    }

    loaded.valid = loaded.segment_count > 0 &&
        loaded.entry >= loaded.lowest_address &&
        loaded.entry < loaded.highest_address;
    if (loaded.valid) {
        g_Status.loaded_elf_count++;
    }
    return loaded.valid;
}

bool RegisterDynamicSymbol(const char* name, uint64_t address) {
    if (!name || address == 0) {
        return false;
    }

    for (uint32_t i = 0; i < kMaxDynamicSymbols; i++) {
        if (!g_DynamicSymbols[i].resolved) {
            g_DynamicSymbols[i].name = name;
            g_DynamicSymbols[i].address = address;
            g_DynamicSymbols[i].resolved = true;
            return true;
        }
    }

    return false;
}

uint64_t ResolveDynamicSymbol(const char* name) {
    for (uint32_t i = 0; i < kMaxDynamicSymbols; i++) {
        if (g_DynamicSymbols[i].resolved && StringEquals(g_DynamicSymbols[i].name, name)) {
            return g_DynamicSymbols[i].address;
        }
    }

    return 0;
}

bool RunElfLoaderSelfTest() {
    uint8_t image[sizeof(Elf64Header) + sizeof(Elf64ProgramHeader) + 16];
    for (uint64_t i = 0; i < sizeof(image); i++) {
        image[i] = 0;
    }

    Elf64Header* header = reinterpret_cast<Elf64Header*>(image);
    header->ident[0] = 0x7F;
    header->ident[1] = 'E';
    header->ident[2] = 'L';
    header->ident[3] = 'F';
    header->ident[4] = kElfClass64;
    header->ident[5] = kElfDataLittleEndian;
    header->type = kElfTypeExecutable;
    header->machine = kElfMachineX86_64;
    header->version = kElfVersionCurrent;
    header->entry = 0x400000;
    header->program_header_offset = sizeof(Elf64Header);
    header->header_size = sizeof(Elf64Header);
    header->program_header_entry_size = sizeof(Elf64ProgramHeader);
    header->program_header_count = 1;

    Elf64ProgramHeader* program = reinterpret_cast<Elf64ProgramHeader*>(image + sizeof(Elf64Header));
    program->type = kElfLoadSegment;
    program->offset = sizeof(Elf64Header) + sizeof(Elf64ProgramHeader);
    program->virtual_address = 0x400000;
    program->physical_address = 0x400000;
    program->file_size = 16;
    program->memory_size = 16;
    program->align = 0x1000;

    UserElfImage loaded;
    return LoadUserElfImage(image, sizeof(image), loaded) &&
        loaded.valid &&
        loaded.entry == 0x400000 &&
        loaded.segment_count == 1;
}

bool RunDynamicLinkerSelfTest() {
    return RegisterDynamicSymbol("write", reinterpret_cast<uint64_t>(&LibcWrite)) &&
        ResolveDynamicSymbol("write") == reinterpret_cast<uint64_t>(&LibcWrite) &&
        ResolveDynamicSymbol("missing") == 0;
}

bool RunLibcSelfTest() {
    const uint64_t before = g_Status.syscall_count;
    const uint64_t written = LibcWrite(1, "libc: ok");
    return written == 8 && g_Status.syscall_count == before + 1;
}

bool RunPosixSelfTest() {
    char buffer[1];
    const int32_t fd = PosixOpen("/tmp/readme", 0);
    if (fd < 3) {
        return false;
    }

    const bool read_ok = PosixRead(fd, buffer, sizeof(buffer)) == 0;
    const bool close_ok = PosixClose(fd) == 0;
    return read_ok && close_ok && PosixClose(fd) == -1;
}

void ShellMain(void*) {
    UserProcess* process = FindUserProcess(g_Status.shell_process_id);
    if (process) {
        process->state = UserProcessState::Running;
    }

    KernelSyscall(static_cast<uint64_t>(SyscallNumber::Write),
        reinterpret_cast<uint64_t>("shell: ready"),
        12,
        0);
    KernelSyscall(static_cast<uint64_t>(SyscallNumber::Write),
        reinterpret_cast<uint64_t>("shell: exit"),
        11,
        0);
    KernelSyscall(static_cast<uint64_t>(SyscallNumber::Exit), g_Status.shell_process_id, 0, 0);
}

bool StartShellProcess() {
    const uint64_t process_id = KernelCreateProcess("user-shell");
    if (process_id == 0 || !RegisterUserProcess(process_id, "user-shell")) {
        return false;
    }

    const uint64_t thread_id = KernelCreateThread(process_id, "shell-main", ShellMain, nullptr);
    if (thread_id == 0) {
        return false;
    }

    g_Status.shell_process_id = process_id;
    g_Status.shell_thread_id = thread_id;
    g_Status.shell_ready = true;
    g_Status.user_process_ready = true;
    return true;
}

} // namespace

uint64_t KernelSyscall(uint64_t number, uint64_t arg0, uint64_t arg1, uint64_t arg2) {
    g_Status.syscall_count++;

    switch (static_cast<SyscallNumber>(number)) {
        case SyscallNumber::Write:
            return SysWrite(reinterpret_cast<const char*>(arg0), arg1);
        case SyscallNumber::Exit:
            return SysExit(arg0, arg1);
        case SyscallNumber::Yield:
            KernelSchedulerYield();
            return 0;
        case SyscallNumber::GetPid:
            static_cast<void>(arg0);
            static_cast<void>(arg1);
            static_cast<void>(arg2);
            return g_Status.shell_process_id;
        case SyscallNumber::Open:
            static_cast<void>(arg2);
            return static_cast<uint64_t>(PosixOpen(reinterpret_cast<const char*>(arg0), static_cast<int32_t>(arg1)));
        case SyscallNumber::Read:
            return PosixRead(static_cast<int32_t>(arg0), reinterpret_cast<char*>(arg1), arg2);
        case SyscallNumber::Close:
            static_cast<void>(arg1);
            static_cast<void>(arg2);
            return static_cast<uint64_t>(PosixClose(static_cast<int32_t>(arg0)));
    }

    static_cast<void>(arg2);
    return 0;
}

bool KernelUserspaceInit() {
    ResetUserspaceStatus();
    g_Status.syscalls_ready = true;
    g_Status.elf_loader_ready = RunElfLoaderSelfTest();
    g_Status.dynamic_linker_ready = RunDynamicLinkerSelfTest();
    g_Status.libc_ready = RunLibcSelfTest();
    g_Status.posix_ready = RunPosixSelfTest();

    if (!StartShellProcess()) {
        return false;
    }

    KernelLog(LogLevel::Info, "Phase 11 userspace initialized");
    KernelSchedulerYield();
    return g_Status.syscalls_ready &&
        g_Status.shell_ready &&
        g_Status.user_process_ready &&
        g_Status.elf_loader_ready &&
        g_Status.dynamic_linker_ready &&
        g_Status.libc_ready &&
        g_Status.posix_ready;
}

const UserspaceStatus& KernelUserspaceStatus() {
    return g_Status;
}

void PrintUserspaceInfo() {
    KernelLog(g_Status.syscalls_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.syscalls_ready ? "Syscall dispatcher ready" : "Syscall dispatcher unavailable");
    KernelLog(g_Status.shell_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.shell_ready ? "Shell process created" : "Shell process unavailable");
    KernelLog(g_Status.user_process_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.user_process_ready ? "User process model ready" : "User process model unavailable");
    KernelLog(g_Status.elf_loader_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.elf_loader_ready ? "ELF userspace loader ready" : "ELF userspace loader unavailable");
    KernelLog(g_Status.dynamic_linker_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.dynamic_linker_ready ? "Dynamic linker scaffold ready" : "Dynamic linker scaffold unavailable");
    KernelLog(g_Status.libc_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.libc_ready ? "libc syscall wrappers ready" : "libc syscall wrappers unavailable");
    KernelLog(g_Status.posix_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.posix_ready ? "POSIX compatibility layer ready" : "POSIX compatibility layer unavailable");
}
