#pragma once

#include <stdint.h>

struct UserspaceStatus {
    bool syscalls_ready;
    bool shell_ready;
    bool user_process_ready;
    uint64_t shell_process_id;
    uint64_t shell_thread_id;
    uint64_t syscall_count;
};

bool KernelUserspaceInit();
const UserspaceStatus& KernelUserspaceStatus();
uint64_t KernelSyscall(uint64_t number, uint64_t arg0, uint64_t arg1, uint64_t arg2);
void PrintUserspaceInfo();
