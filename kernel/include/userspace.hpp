#pragma once

#include <stdint.h>

struct UserspaceStatus {
    bool syscalls_ready;
    bool shell_ready;
    bool user_process_ready;
    bool elf_loader_ready;
    bool dynamic_linker_ready;
    bool libc_ready;
    bool posix_ready;
    bool init_ready;
    bool dynamic_loader_cache_ready;
    bool environment_ready;
    bool shared_libraries_ready;
    bool signals_ready;
    bool pipes_ready;
    bool pty_ready;
    uint64_t shell_process_id;
    uint64_t shell_thread_id;
    uint64_t init_process_id;
    uint64_t init_thread_id;
    uint64_t syscall_count;
    uint64_t loaded_elf_count;
    uint32_t cached_loader_entry_count;
    uint32_t environment_variable_count;
    uint32_t shared_library_count;
    uint32_t delivered_signal_count;
    uint32_t pipe_count;
    uint32_t pty_count;
};

bool KernelUserspaceInit();
const UserspaceStatus& KernelUserspaceStatus();
uint64_t KernelSyscall(uint64_t number, uint64_t arg0, uint64_t arg1, uint64_t arg2);
void PrintUserspaceInfo();
