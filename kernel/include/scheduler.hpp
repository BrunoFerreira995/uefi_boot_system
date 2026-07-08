#pragma once

#include <stdint.h>

bool KernelSchedulerInit();
uint64_t KernelCreateProcess(const char* name);
uint64_t KernelCreateThread(uint64_t process_id, const char* name, void (*entry)(void*), void* argument);
void KernelSchedulerYield();
void KernelSchedulerRunSelfTest();
void PrintSchedulerInfo();

