#pragma once

#include <stdint.h>

bool KernelSchedulerInit();
uint64_t KernelCreateProcess(const char* name);
uint64_t KernelCreateThread(uint64_t process_id, const char* name, void (*entry)(void*), void* argument);
bool KernelTerminateProcess(uint64_t process_id, int32_t exit_code);
uint64_t KernelForkProcess(uint64_t parent_process_id);
bool KernelExecProcess(uint64_t process_id, const char* image_name, void (*entry)(void*), void* argument);
uint64_t KernelCloneThread(uint64_t process_id, void (*entry)(void*), void* argument);
uint64_t KernelPthreadCreate(uint64_t process_id, void (*entry)(void*), void* argument);
bool KernelPthreadJoin(uint64_t thread_id);
bool KernelFutexWait(uint32_t* address, uint32_t expected);
uint32_t KernelFutexWake(uint32_t* address, uint32_t count);
int32_t KernelEpollCreate();
bool KernelEpollAdd(int32_t epoll_fd, int32_t watched_fd);
uint32_t KernelEpollWait(int32_t epoll_fd, int32_t* ready, uint32_t capacity);
int32_t KernelEventFd(uint64_t initial_value);
bool KernelEventFdWrite(int32_t fd, uint64_t value);
bool KernelEventFdRead(int32_t fd, uint64_t& value);
int32_t KernelTimerFd(uint64_t initial_ticks, uint64_t interval_ticks);
bool KernelTimerFdRead(int32_t fd, uint64_t& expirations);
bool KernelSetThreadPriority(uint64_t thread_id, uint8_t priority);
void KernelThreadSleep(uint64_t ticks);
void KernelSchedulerTimerTick();
bool KernelSendSignal(uint64_t thread_id, uint32_t signal);
bool KernelIpcSend(uint64_t process_id, uint64_t value);
bool KernelIpcReceive(uint64_t process_id, uint64_t& value);
uint64_t KernelMutexCreate();
bool KernelMutexLock(uint64_t mutex_id);
bool KernelMutexUnlock(uint64_t mutex_id);
void KernelSchedulerYield();
void KernelSchedulerRunSelfTest();
void PrintSchedulerInfo();
