#pragma once

#include <stdint.h>

struct SecurityStatus {
    bool users_ready;
    bool permissions_ready;
    bool access_control_ready;
    bool process_isolation_ready;
    bool virtual_memory_protection_ready;
    bool aslr_ready;
    bool stack_canaries_ready;
    bool nx_pages_ready;
    bool secure_random_ready;
    bool kaslr_ready;
    uint32_t user_count;
    uint32_t policy_count;
};

bool KernelSecurityInit();
const SecurityStatus& KernelSecurityStatus();
void PrintSecurityInfo();
