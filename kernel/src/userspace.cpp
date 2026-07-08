#include "userspace.hpp"

#include "kernel.hpp"
#include "scheduler.hpp"

namespace {

enum class SyscallNumber : uint64_t {
    Write = 1,
    Exit = 2,
    Yield = 3,
    GetPid = 4,
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

static constexpr uint64_t kMaxUserProcesses = 8;

UserspaceStatus g_Status {};
UserProcess g_UserProcesses[kMaxUserProcesses];

void ResetUserspaceStatus() {
    g_Status.syscalls_ready = false;
    g_Status.shell_ready = false;
    g_Status.user_process_ready = false;
    g_Status.shell_process_id = 0;
    g_Status.shell_thread_id = 0;
    g_Status.syscall_count = 0;

    for (uint64_t i = 0; i < kMaxUserProcesses; i++) {
        g_UserProcesses[i].process_id = 0;
        g_UserProcesses[i].name = nullptr;
        g_UserProcesses[i].state = UserProcessState::Empty;
        g_UserProcesses[i].ring3_intent = false;
    }
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
    }

    static_cast<void>(arg2);
    return 0;
}

bool KernelUserspaceInit() {
    ResetUserspaceStatus();
    g_Status.syscalls_ready = true;

    if (!StartShellProcess()) {
        return false;
    }

    KernelLog(LogLevel::Info, "Phase 9 userspace initialized");
    KernelSchedulerYield();
    return g_Status.syscalls_ready && g_Status.shell_ready && g_Status.user_process_ready;
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
}
