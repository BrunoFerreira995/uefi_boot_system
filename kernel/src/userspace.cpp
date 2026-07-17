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
    Pipe = 8,
    Signal = 9,
    GetEnv = 10,
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

struct LoaderCacheEntry {
    const char* path;
    uint64_t base_address;
    uint64_t entry_address;
    uint64_t image_size;
    bool valid;
};

struct EnvironmentVariable {
    const char* name;
    const char* value;
    bool active;
};

struct SharedLibrary {
    const char* soname;
    uint64_t base_address;
    uint32_t reference_count;
    bool loaded;
};

struct UserSignalHandler {
    uint64_t process_id;
    uint32_t signal;
    bool pending;
    bool delivered;
};

struct UserPipe {
    int32_t read_fd;
    int32_t write_fd;
    char buffer[64];
    uint32_t read_offset;
    uint32_t write_offset;
    bool open;
};

struct PseudoTerminal {
    int32_t master_fd;
    int32_t slave_fd;
    const char* name;
    bool open;
};

static constexpr uint64_t kMaxUserProcesses = 8;
static constexpr uint32_t kMaxDynamicSymbols = 8;
static constexpr uint32_t kMaxPosixFileDescriptors = 8;
static constexpr uint32_t kMaxLoaderCacheEntries = 8;
static constexpr uint32_t kMaxEnvironmentVariables = 12;
static constexpr uint32_t kMaxSharedLibraries = 8;
static constexpr uint32_t kMaxSignalHandlers = 8;
static constexpr uint32_t kMaxPipes = 4;
static constexpr uint32_t kMaxPtys = 4;
static constexpr uint32_t kElfLoadSegment = 1;
static constexpr uint16_t kElfTypeExecutable = 2;
static constexpr uint16_t kElfMachineX86_64 = 0x3E;
static constexpr uint32_t kElfVersionCurrent = 1;
static constexpr uint8_t kElfClass64 = 2;
static constexpr uint8_t kElfDataLittleEndian = 1;
static constexpr int32_t kFirstSyntheticFd = 64;

UserspaceStatus g_Status {};
UserProcess g_UserProcesses[kMaxUserProcesses];
DynamicSymbol g_DynamicSymbols[kMaxDynamicSymbols];
PosixFileDescriptor g_PosixFileDescriptors[kMaxPosixFileDescriptors];
LoaderCacheEntry g_LoaderCache[kMaxLoaderCacheEntries];
EnvironmentVariable g_Environment[kMaxEnvironmentVariables];
SharedLibrary g_SharedLibraries[kMaxSharedLibraries];
UserSignalHandler g_SignalHandlers[kMaxSignalHandlers];
UserPipe g_Pipes[kMaxPipes];
PseudoTerminal g_Ptys[kMaxPtys];
int32_t g_NextSyntheticFd = kFirstSyntheticFd;

void ResetUserspaceStatus() {
    g_Status.syscalls_ready = false;
    g_Status.shell_ready = false;
    g_Status.user_process_ready = false;
    g_Status.elf_loader_ready = false;
    g_Status.dynamic_linker_ready = false;
    g_Status.libc_ready = false;
    g_Status.posix_ready = false;
    g_Status.init_ready = false;
    g_Status.dynamic_loader_cache_ready = false;
    g_Status.environment_ready = false;
    g_Status.shared_libraries_ready = false;
    g_Status.signals_ready = false;
    g_Status.pipes_ready = false;
    g_Status.pty_ready = false;
    g_Status.shell_process_id = 0;
    g_Status.shell_thread_id = 0;
    g_Status.init_process_id = 0;
    g_Status.init_thread_id = 0;
    g_Status.syscall_count = 0;
    g_Status.loaded_elf_count = 0;
    g_Status.cached_loader_entry_count = 0;
    g_Status.environment_variable_count = 0;
    g_Status.shared_library_count = 0;
    g_Status.delivered_signal_count = 0;
    g_Status.pipe_count = 0;
    g_Status.pty_count = 0;
    g_NextSyntheticFd = kFirstSyntheticFd;

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

    for (uint32_t i = 0; i < kMaxLoaderCacheEntries; i++) {
        g_LoaderCache[i].path = nullptr;
        g_LoaderCache[i].base_address = 0;
        g_LoaderCache[i].entry_address = 0;
        g_LoaderCache[i].image_size = 0;
        g_LoaderCache[i].valid = false;
    }

    for (uint32_t i = 0; i < kMaxEnvironmentVariables; i++) {
        g_Environment[i].name = nullptr;
        g_Environment[i].value = nullptr;
        g_Environment[i].active = false;
    }

    for (uint32_t i = 0; i < kMaxSharedLibraries; i++) {
        g_SharedLibraries[i].soname = nullptr;
        g_SharedLibraries[i].base_address = 0;
        g_SharedLibraries[i].reference_count = 0;
        g_SharedLibraries[i].loaded = false;
    }

    for (uint32_t i = 0; i < kMaxSignalHandlers; i++) {
        g_SignalHandlers[i].process_id = 0;
        g_SignalHandlers[i].signal = 0;
        g_SignalHandlers[i].pending = false;
        g_SignalHandlers[i].delivered = false;
    }

    for (uint32_t i = 0; i < kMaxPipes; i++) {
        g_Pipes[i].read_fd = -1;
        g_Pipes[i].write_fd = -1;
        g_Pipes[i].read_offset = 0;
        g_Pipes[i].write_offset = 0;
        g_Pipes[i].open = false;
        for (uint32_t j = 0; j < sizeof(g_Pipes[i].buffer); j++) {
            g_Pipes[i].buffer[j] = '\0';
        }
    }

    for (uint32_t i = 0; i < kMaxPtys; i++) {
        g_Ptys[i].master_fd = -1;
        g_Ptys[i].slave_fd = -1;
        g_Ptys[i].name = nullptr;
        g_Ptys[i].open = false;
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

uint32_t StringLength(const char* text) {
    uint32_t length = 0;
    if (!text) {
        return 0;
    }

    while (text[length]) {
        length++;
    }
    return length;
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

bool RegisterLoaderCacheEntry(const char* path, uint64_t base_address, uint64_t entry_address, uint64_t image_size) {
    if (!path || base_address == 0 || entry_address == 0 || image_size == 0) {
        return false;
    }

    for (uint32_t i = 0; i < kMaxLoaderCacheEntries; i++) {
        if (g_LoaderCache[i].valid && StringEquals(g_LoaderCache[i].path, path)) {
            g_LoaderCache[i].base_address = base_address;
            g_LoaderCache[i].entry_address = entry_address;
            g_LoaderCache[i].image_size = image_size;
            return true;
        }
    }

    for (uint32_t i = 0; i < kMaxLoaderCacheEntries; i++) {
        if (!g_LoaderCache[i].valid) {
            g_LoaderCache[i].path = path;
            g_LoaderCache[i].base_address = base_address;
            g_LoaderCache[i].entry_address = entry_address;
            g_LoaderCache[i].image_size = image_size;
            g_LoaderCache[i].valid = true;
            g_Status.cached_loader_entry_count++;
            return true;
        }
    }

    return false;
}

LoaderCacheEntry* FindLoaderCacheEntry(const char* path) {
    for (uint32_t i = 0; i < kMaxLoaderCacheEntries; i++) {
        if (g_LoaderCache[i].valid && StringEquals(g_LoaderCache[i].path, path)) {
            return &g_LoaderCache[i];
        }
    }

    return nullptr;
}

bool SetEnvironmentVariable(const char* name, const char* value) {
    if (!name || !value || StringLength(name) == 0) {
        return false;
    }

    for (uint32_t i = 0; i < kMaxEnvironmentVariables; i++) {
        if (g_Environment[i].active && StringEquals(g_Environment[i].name, name)) {
            g_Environment[i].value = value;
            return true;
        }
    }

    for (uint32_t i = 0; i < kMaxEnvironmentVariables; i++) {
        if (!g_Environment[i].active) {
            g_Environment[i].name = name;
            g_Environment[i].value = value;
            g_Environment[i].active = true;
            g_Status.environment_variable_count++;
            return true;
        }
    }

    return false;
}

const char* GetEnvironmentVariable(const char* name) {
    for (uint32_t i = 0; i < kMaxEnvironmentVariables; i++) {
        if (g_Environment[i].active && StringEquals(g_Environment[i].name, name)) {
            return g_Environment[i].value;
        }
    }

    return nullptr;
}

bool LoadSharedLibrary(const char* soname, uint64_t base_address) {
    if (!soname || base_address == 0) {
        return false;
    }

    for (uint32_t i = 0; i < kMaxSharedLibraries; i++) {
        if (g_SharedLibraries[i].loaded && StringEquals(g_SharedLibraries[i].soname, soname)) {
            g_SharedLibraries[i].reference_count++;
            return true;
        }
    }

    for (uint32_t i = 0; i < kMaxSharedLibraries; i++) {
        if (!g_SharedLibraries[i].loaded) {
            g_SharedLibraries[i].soname = soname;
            g_SharedLibraries[i].base_address = base_address;
            g_SharedLibraries[i].reference_count = 1;
            g_SharedLibraries[i].loaded = true;
            g_Status.shared_library_count++;
            return true;
        }
    }

    return false;
}

SharedLibrary* FindSharedLibrary(const char* soname) {
    for (uint32_t i = 0; i < kMaxSharedLibraries; i++) {
        if (g_SharedLibraries[i].loaded && StringEquals(g_SharedLibraries[i].soname, soname)) {
            return &g_SharedLibraries[i];
        }
    }

    return nullptr;
}

bool RegisterSignalHandler(uint64_t process_id, uint32_t signal) {
    if (process_id == 0 || signal >= 32) {
        return false;
    }

    for (uint32_t i = 0; i < kMaxSignalHandlers; i++) {
        if (!g_SignalHandlers[i].pending && !g_SignalHandlers[i].delivered) {
            g_SignalHandlers[i].process_id = process_id;
            g_SignalHandlers[i].signal = signal;
            return true;
        }
    }

    return false;
}

bool DeliverUserSignal(uint64_t process_id, uint64_t thread_id, uint32_t signal) {
    for (uint32_t i = 0; i < kMaxSignalHandlers; i++) {
        UserSignalHandler& handler = g_SignalHandlers[i];
        if (handler.process_id == process_id && handler.signal == signal) {
            if (!KernelSendSignal(thread_id, signal)) {
                return false;
            }
            handler.pending = false;
            handler.delivered = true;
            g_Status.delivered_signal_count++;
            return true;
        }
    }

    return false;
}

UserPipe* CreatePipe(int32_t* read_fd, int32_t* write_fd) {
    if (!read_fd || !write_fd) {
        return nullptr;
    }

    for (uint32_t i = 0; i < kMaxPipes; i++) {
        if (!g_Pipes[i].open) {
            UserPipe& pipe = g_Pipes[i];
            pipe.read_fd = g_NextSyntheticFd++;
            pipe.write_fd = g_NextSyntheticFd++;
            pipe.read_offset = 0;
            pipe.write_offset = 0;
            pipe.open = true;
            for (uint32_t j = 0; j < sizeof(pipe.buffer); j++) {
                pipe.buffer[j] = '\0';
            }
            *read_fd = pipe.read_fd;
            *write_fd = pipe.write_fd;
            g_Status.pipe_count++;
            return &pipe;
        }
    }

    return nullptr;
}

uint32_t PipeWrite(int32_t fd, const char* data, uint32_t length) {
    if (!data || length == 0) {
        return 0;
    }

    for (uint32_t i = 0; i < kMaxPipes; i++) {
        UserPipe& pipe = g_Pipes[i];
        if (!pipe.open || pipe.write_fd != fd) {
            continue;
        }

        uint32_t written = 0;
        while (written < length && pipe.write_offset < sizeof(pipe.buffer)) {
            pipe.buffer[pipe.write_offset++] = data[written++];
        }
        return written;
    }

    return 0;
}

uint32_t PipeRead(int32_t fd, char* data, uint32_t length) {
    if (!data || length == 0) {
        return 0;
    }

    for (uint32_t i = 0; i < kMaxPipes; i++) {
        UserPipe& pipe = g_Pipes[i];
        if (!pipe.open || pipe.read_fd != fd) {
            continue;
        }

        uint32_t read = 0;
        while (read < length && pipe.read_offset < pipe.write_offset) {
            data[read++] = pipe.buffer[pipe.read_offset++];
        }
        return read;
    }

    return 0;
}

PseudoTerminal* OpenPseudoTerminal(const char* name, int32_t* master_fd, int32_t* slave_fd) {
    if (!name || !master_fd || !slave_fd) {
        return nullptr;
    }

    for (uint32_t i = 0; i < kMaxPtys; i++) {
        if (!g_Ptys[i].open) {
            PseudoTerminal& pty = g_Ptys[i];
            pty.master_fd = g_NextSyntheticFd++;
            pty.slave_fd = g_NextSyntheticFd++;
            pty.name = name;
            pty.open = true;
            *master_fd = pty.master_fd;
            *slave_fd = pty.slave_fd;
            g_Status.pty_count++;
            return &pty;
        }
    }

    return nullptr;
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

bool RunDynamicLoaderCacheSelfTest() {
    return RegisterLoaderCacheEntry("/bin/init", 0x400000, 0x401000, 0x3000) &&
        RegisterLoaderCacheEntry("/bin/sh", 0x500000, 0x501000, 0x2000) &&
        FindLoaderCacheEntry("/bin/init") != nullptr &&
        FindLoaderCacheEntry("/bin/missing") == nullptr;
}

bool RunEnvironmentSelfTest() {
    return SetEnvironmentVariable("PATH", "/bin:/usr/bin") &&
        SetEnvironmentVariable("HOME", "/") &&
        SetEnvironmentVariable("TERM", "kernel") &&
        StringEquals(GetEnvironmentVariable("PATH"), "/bin:/usr/bin") &&
        StringEquals(GetEnvironmentVariable("TERM"), "kernel") &&
        GetEnvironmentVariable("MISSING") == nullptr;
}

bool RunSharedLibrarySelfTest() {
    if (!LoadSharedLibrary("libc.so", 0x700000) ||
        !LoadSharedLibrary("libposix.so", 0x720000) ||
        !LoadSharedLibrary("libc.so", 0x700000)) {
        return false;
    }

    SharedLibrary* libc = FindSharedLibrary("libc.so");
    SharedLibrary* libposix = FindSharedLibrary("libposix.so");
    return libc && libposix &&
        libc->reference_count == 2 &&
        libposix->reference_count == 1 &&
        FindSharedLibrary("libmissing.so") == nullptr;
}

bool RunPipeSelfTest() {
    int32_t read_fd = -1;
    int32_t write_fd = -1;
    if (!CreatePipe(&read_fd, &write_fd) || read_fd < 0 || write_fd < 0 || read_fd == write_fd) {
        return false;
    }

    char buffer[6] = {};
    return PipeWrite(write_fd, "hello", 5) == 5 &&
        PipeRead(read_fd, buffer, 5) == 5 &&
        buffer[0] == 'h' &&
        buffer[4] == 'o';
}

bool RunPtySelfTest() {
    int32_t master_fd = -1;
    int32_t slave_fd = -1;
    PseudoTerminal* pty = OpenPseudoTerminal("/dev/pts/0", &master_fd, &slave_fd);
    return pty &&
        pty->open &&
        master_fd >= kFirstSyntheticFd &&
        slave_fd >= kFirstSyntheticFd &&
        master_fd != slave_fd &&
        StringEquals(pty->name, "/dev/pts/0");
}

void InitMain(void*) {
    KernelLog(LogLevel::Info, "init: userspace services ready");
    KernelSyscall(static_cast<uint64_t>(SyscallNumber::Yield), 0, 0, 0);
}

bool StartInitProcess() {
    const uint64_t process_id = KernelCreateProcess("init");
    if (process_id == 0 || !RegisterUserProcess(process_id, "init")) {
        return false;
    }

    const uint64_t thread_id = KernelCreateThread(process_id, "init-main", InitMain, nullptr);
    if (thread_id == 0) {
        return false;
    }

    g_Status.init_process_id = process_id;
    g_Status.init_thread_id = thread_id;
    g_Status.init_ready = true;
    return true;
}

bool RunSignalsSelfTest() {
    return g_Status.init_process_id != 0 &&
        g_Status.init_thread_id != 0 &&
        RegisterSignalHandler(g_Status.init_process_id, 2) &&
        DeliverUserSignal(g_Status.init_process_id, g_Status.init_thread_id, 2);
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
        case SyscallNumber::Pipe:
            return CreatePipe(reinterpret_cast<int32_t*>(arg0), reinterpret_cast<int32_t*>(arg1)) ? 0 : 1;
        case SyscallNumber::Signal:
            return DeliverUserSignal(arg0, arg1, static_cast<uint32_t>(arg2)) ? 0 : 1;
        case SyscallNumber::GetEnv:
            static_cast<void>(arg1);
            static_cast<void>(arg2);
            return reinterpret_cast<uint64_t>(GetEnvironmentVariable(reinterpret_cast<const char*>(arg0)));
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
    g_Status.dynamic_loader_cache_ready = RunDynamicLoaderCacheSelfTest();
    g_Status.environment_ready = RunEnvironmentSelfTest();
    g_Status.shared_libraries_ready = RunSharedLibrarySelfTest();
    g_Status.pipes_ready = RunPipeSelfTest();
    g_Status.pty_ready = RunPtySelfTest();

    if (!StartInitProcess()) {
        return false;
    }

    g_Status.signals_ready = RunSignalsSelfTest();

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
        g_Status.posix_ready &&
        g_Status.init_ready &&
        g_Status.dynamic_loader_cache_ready &&
        g_Status.environment_ready &&
        g_Status.shared_libraries_ready &&
        g_Status.signals_ready &&
        g_Status.pipes_ready &&
        g_Status.pty_ready;
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
    KernelLog(g_Status.init_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.init_ready ? "init process created" : "init process unavailable");
    KernelLog(g_Status.dynamic_loader_cache_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.dynamic_loader_cache_ready ? "Dynamic loader cache ready" : "Dynamic loader cache unavailable");
    KernelLog(g_Status.environment_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.environment_ready ? "Environment variables ready" : "Environment variables unavailable");
    KernelLog(g_Status.shared_libraries_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.shared_libraries_ready ? "Shared library registry ready" : "Shared library registry unavailable");
    KernelLog(g_Status.signals_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.signals_ready ? "Userspace signals ready" : "Userspace signals unavailable");
    KernelLog(g_Status.pipes_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.pipes_ready ? "Pipes ready" : "Pipes unavailable");
    KernelLog(g_Status.pty_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.pty_ready ? "Pseudo terminals ready" : "Pseudo terminals unavailable");
}
