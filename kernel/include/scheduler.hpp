#pragma once

#include <stdint.h>

bool KernelSchedulerInit();
uint64_t KernelCreateProcess(const char* name);
uint64_t KernelCreateThread(uint64_t process_id, const char* name, void (*entry)(void*), void* argument);
bool KernelSetThreadPriority(uint64_t thread_id, uint8_t priority);
void KernelThreadSleep(uint64_t ticks);
bool KernelSendSignal(uint64_t thread_id, uint32_t signal);
bool KernelIpcSend(uint64_t process_id, uint64_t value);
bool KernelIpcReceive(uint64_t process_id, uint64_t& value);
uint64_t KernelMutexCreate();
bool KernelMutexLock(uint64_t mutex_id);
bool KernelMutexUnlock(uint64_t mutex_id);
void KernelSchedulerYield();
void KernelSchedulerRunSelfTest();
void PrintSchedulerInfo();
