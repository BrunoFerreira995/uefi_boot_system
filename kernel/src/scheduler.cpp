#include "scheduler.hpp"
#include "kernel.hpp"

extern "C" void SchedulerContextSwitch(void* previous_context, void* next_context);

namespace {

static constexpr uint64_t kMaxProcesses = 8;
static constexpr uint64_t kMaxThreads = 16;
static constexpr uint64_t kThreadStackSize = 8192;
static constexpr uint64_t kKernelProcessId = 1;
static constexpr uint64_t kIpcQueueSize = 8;
static constexpr uint64_t kMaxMutexes = 8;
static constexpr uint8_t kDefaultThreadPriority = 8;
static constexpr uint8_t kMaxThreadPriority = 31;

enum class ThreadState : uint8_t {
    Empty,
    Ready,
    Running,
    Sleeping,
    Blocked,
    Finished,
};

struct ThreadContext {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t rsp;
};

struct Process {
    uint64_t id;
    const char* name;
    bool active;
    uint64_t ipc_queue[kIpcQueueSize];
    uint64_t ipc_head;
    uint64_t ipc_tail;
    uint64_t ipc_count;
};

struct Thread {
    uint64_t id;
    uint64_t process_id;
    const char* name;
    ThreadState state;
    ThreadContext context;
    uint8_t* stack_base;
    uint64_t stack_size;
    void (*entry)(void*);
    void* argument;
    uint64_t run_count;
    uint64_t wake_tick;
    uint32_t pending_signals;
    uint32_t handled_signals;
    uint8_t priority;
    uint64_t waiting_mutex_id;
};

struct KernelMutex {
    uint64_t id;
    bool active;
    bool locked;
    uint64_t owner_thread_id;
};

Process g_Processes[kMaxProcesses];
Thread g_Threads[kMaxThreads];
KernelMutex g_Mutexes[kMaxMutexes];
Thread* g_CurrentThread = nullptr;
uint64_t g_NextProcessId = kKernelProcessId;
uint64_t g_NextThreadId = 1;
uint64_t g_NextMutexId = 1;
uint64_t g_SchedulerTick = 0;
uint64_t g_SignalsDelivered = 0;

struct DemoThreadArgument {
    const char* name;
    uint64_t steps;
};

DemoThreadArgument g_DemoThreadA = {"scheduler worker A", 2};
DemoThreadArgument g_DemoThreadB = {"scheduler worker B", 2};

void ClearBytes(void* ptr, uint64_t size) {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(ptr);
    for (uint64_t i = 0; i < size; i++) {
        bytes[i] = 0;
    }
}

uint64_t AlignDown(uint64_t value, uint64_t alignment) {
    return value & ~(alignment - 1);
}

Process* FindProcess(uint64_t id) {
    for (uint64_t i = 0; i < kMaxProcesses; i++) {
        if (g_Processes[i].active && g_Processes[i].id == id) {
            return &g_Processes[i];
        }
    }

    return nullptr;
}

Thread* FindThreadSlot() {
    for (uint64_t i = 0; i < kMaxThreads; i++) {
        if (g_Threads[i].state == ThreadState::Empty) {
            return &g_Threads[i];
        }
    }

    return nullptr;
}

Thread* FindThread(uint64_t id) {
    for (uint64_t i = 0; i < kMaxThreads; i++) {
        if (g_Threads[i].state != ThreadState::Empty && g_Threads[i].id == id) {
            return &g_Threads[i];
        }
    }

    return nullptr;
}

KernelMutex* FindMutex(uint64_t id) {
    for (uint64_t i = 0; i < kMaxMutexes; i++) {
        if (g_Mutexes[i].active && g_Mutexes[i].id == id) {
            return &g_Mutexes[i];
        }
    }

    return nullptr;
}

uint64_t ThreadIndex(const Thread* thread) {
    return static_cast<uint64_t>(thread - g_Threads);
}

Thread* FindNextRunnable() {
    if (!g_CurrentThread) {
        return nullptr;
    }

    for (uint64_t i = 0; i < kMaxThreads; i++) {
        Thread& thread = g_Threads[i];
        if (thread.state == ThreadState::Sleeping && thread.wake_tick <= g_SchedulerTick) {
            thread.state = ThreadState::Ready;
            thread.wake_tick = 0;
        }
    }

    const uint64_t start = ThreadIndex(g_CurrentThread);
    Thread* selected = nullptr;

    for (uint64_t offset = 1; offset <= kMaxThreads; offset++) {
        Thread& candidate = g_Threads[(start + offset) % kMaxThreads];
        if (candidate.state == ThreadState::Ready) {
            if (!selected || candidate.priority > selected->priority) {
                selected = &candidate;
            }
        }
    }

    return selected;
}

uint64_t RunnableWorkerCount() {
    uint64_t count = 0;

    for (uint64_t i = 0; i < kMaxThreads; i++) {
        const Thread& thread = g_Threads[i];
        if (&thread != g_CurrentThread &&
            thread.state != ThreadState::Empty &&
            thread.state != ThreadState::Finished &&
            thread.state != ThreadState::Blocked) {
            count++;
        }
    }

    return count;
}

void DeliverSignals(Thread& thread) {
    if (thread.pending_signals == 0) {
        return;
    }

    thread.handled_signals |= thread.pending_signals;
    thread.pending_signals = 0;
    g_SignalsDelivered++;
}

void ThreadTrampoline() {
    Thread* thread = g_CurrentThread;
    if (!thread || !thread->entry) {
        KernelPanic("Scheduler entered invalid thread");
    }

    DeliverSignals(*thread);
    thread->entry(thread->argument);
    thread->state = ThreadState::Finished;
    KernelSchedulerYield();
    KernelPanic("Finished thread resumed");
}

void DemoThreadMain(void* argument) {
    DemoThreadArgument* demo = reinterpret_cast<DemoThreadArgument*>(argument);
    for (uint64_t i = 0; i < demo->steps; i++) {
        KernelLog(LogLevel::Info, demo->name);
        KernelSchedulerYield();
    }
}

void PrepareInitialContext(Thread& thread) {
    const uint64_t raw_top = reinterpret_cast<uint64_t>(thread.stack_base + thread.stack_size);
    uint64_t* stack = reinterpret_cast<uint64_t*>(AlignDown(raw_top, 16) - 16);
    stack[0] = reinterpret_cast<uint64_t>(ThreadTrampoline);

    ClearBytes(&thread.context, sizeof(thread.context));
    thread.context.rsp = reinterpret_cast<uint64_t>(stack);
}

} // namespace

bool KernelSchedulerInit() {
    ClearBytes(g_Processes, sizeof(g_Processes));
    ClearBytes(g_Threads, sizeof(g_Threads));

    g_Processes[0].id = g_NextProcessId++;
    g_Processes[0].name = "kernel";
    g_Processes[0].active = true;
    g_Processes[0].ipc_head = 0;
    g_Processes[0].ipc_tail = 0;
    g_Processes[0].ipc_count = 0;

    g_Threads[0].id = g_NextThreadId++;
    g_Threads[0].process_id = kKernelProcessId;
    g_Threads[0].name = "bootstrap";
    g_Threads[0].state = ThreadState::Running;
    g_Threads[0].priority = kDefaultThreadPriority;
    g_CurrentThread = &g_Threads[0];
    ClearBytes(g_Mutexes, sizeof(g_Mutexes));
    g_NextMutexId = 1;
    g_SchedulerTick = 0;
    g_SignalsDelivered = 0;

    KernelLog(LogLevel::Info, "Scheduler bootstrap thread registered");
    return true;
}

uint64_t KernelCreateProcess(const char* name) {
    for (uint64_t i = 0; i < kMaxProcesses; i++) {
        if (!g_Processes[i].active) {
            g_Processes[i].id = g_NextProcessId++;
            g_Processes[i].name = name;
            g_Processes[i].active = true;
            g_Processes[i].ipc_head = 0;
            g_Processes[i].ipc_tail = 0;
            g_Processes[i].ipc_count = 0;
            for (uint64_t message = 0; message < kIpcQueueSize; message++) {
                g_Processes[i].ipc_queue[message] = 0;
            }
            return g_Processes[i].id;
        }
    }

    return 0;
}

uint64_t KernelCreateThread(uint64_t process_id, const char* name, void (*entry)(void*), void* argument) {
    if (!FindProcess(process_id) || !entry) {
        return 0;
    }

    Thread* thread = FindThreadSlot();
    if (!thread) {
        return 0;
    }

    void* stack = KernelAllocate(kThreadStackSize, 16);
    if (!stack) {
        return 0;
    }

    thread->id = g_NextThreadId++;
    thread->process_id = process_id;
    thread->name = name;
    thread->state = ThreadState::Ready;
    thread->stack_base = reinterpret_cast<uint8_t*>(stack);
    thread->stack_size = kThreadStackSize;
    thread->entry = entry;
    thread->argument = argument;
    thread->run_count = 0;
    thread->wake_tick = 0;
    thread->pending_signals = 0;
    thread->handled_signals = 0;
    thread->priority = kDefaultThreadPriority;
    thread->waiting_mutex_id = 0;
    PrepareInitialContext(*thread);
    return thread->id;
}

bool KernelSetThreadPriority(uint64_t thread_id, uint8_t priority) {
    Thread* thread = FindThread(thread_id);
    if (!thread || priority > kMaxThreadPriority) {
        return false;
    }

    thread->priority = priority;
    return true;
}

void KernelThreadSleep(uint64_t ticks) {
    if (!g_CurrentThread || ticks == 0) {
        return;
    }

    g_CurrentThread->wake_tick = g_SchedulerTick + ticks;
    g_CurrentThread->state = ThreadState::Sleeping;
    KernelSchedulerYield();
}

bool KernelSendSignal(uint64_t thread_id, uint32_t signal) {
    if (signal >= 32) {
        return false;
    }

    Thread* thread = FindThread(thread_id);
    if (!thread) {
        return false;
    }

    thread->pending_signals |= (1u << signal);
    if (thread->state == ThreadState::Sleeping || thread->state == ThreadState::Blocked) {
        thread->state = ThreadState::Ready;
        thread->wake_tick = 0;
        thread->waiting_mutex_id = 0;
    }
    return true;
}

bool KernelIpcSend(uint64_t process_id, uint64_t value) {
    Process* process = FindProcess(process_id);
    if (!process || process->ipc_count >= kIpcQueueSize) {
        return false;
    }

    process->ipc_queue[process->ipc_tail] = value;
    process->ipc_tail = (process->ipc_tail + 1) % kIpcQueueSize;
    process->ipc_count++;
    return true;
}

bool KernelIpcReceive(uint64_t process_id, uint64_t& value) {
    Process* process = FindProcess(process_id);
    if (!process || process->ipc_count == 0) {
        return false;
    }

    value = process->ipc_queue[process->ipc_head];
    process->ipc_head = (process->ipc_head + 1) % kIpcQueueSize;
    process->ipc_count--;
    return true;
}

uint64_t KernelMutexCreate() {
    for (uint64_t i = 0; i < kMaxMutexes; i++) {
        if (!g_Mutexes[i].active) {
            g_Mutexes[i].id = g_NextMutexId++;
            g_Mutexes[i].active = true;
            g_Mutexes[i].locked = false;
            g_Mutexes[i].owner_thread_id = 0;
            return g_Mutexes[i].id;
        }
    }

    return 0;
}

bool KernelMutexLock(uint64_t mutex_id) {
    KernelMutex* mutex = FindMutex(mutex_id);
    if (!mutex || !g_CurrentThread) {
        return false;
    }

    if (!mutex->locked) {
        mutex->locked = true;
        mutex->owner_thread_id = g_CurrentThread->id;
        return true;
    }

    if (mutex->owner_thread_id == g_CurrentThread->id) {
        return true;
    }

    g_CurrentThread->state = ThreadState::Blocked;
    g_CurrentThread->waiting_mutex_id = mutex_id;
    KernelSchedulerYield();
    return mutex->owner_thread_id == g_CurrentThread->id;
}

bool KernelMutexUnlock(uint64_t mutex_id) {
    KernelMutex* mutex = FindMutex(mutex_id);
    if (!mutex || !g_CurrentThread || mutex->owner_thread_id != g_CurrentThread->id) {
        return false;
    }

    Thread* next_owner = nullptr;
    for (uint64_t i = 0; i < kMaxThreads; i++) {
        Thread& thread = g_Threads[i];
        if (thread.state == ThreadState::Blocked && thread.waiting_mutex_id == mutex_id) {
            if (!next_owner || thread.priority > next_owner->priority) {
                next_owner = &thread;
            }
        }
    }

    if (next_owner) {
        next_owner->state = ThreadState::Ready;
        next_owner->waiting_mutex_id = 0;
        mutex->owner_thread_id = next_owner->id;
    } else {
        mutex->locked = false;
        mutex->owner_thread_id = 0;
    }

    return true;
}

void KernelSchedulerYield() {
    g_SchedulerTick++;
    if (g_CurrentThread) {
        DeliverSignals(*g_CurrentThread);
    }

    Thread* next = FindNextRunnable();
    if (!next || next == g_CurrentThread) {
        if (g_CurrentThread && g_CurrentThread->state == ThreadState::Sleeping) {
            g_CurrentThread->state = ThreadState::Ready;
            g_CurrentThread->wake_tick = 0;
        }
        return;
    }

    Thread* previous = g_CurrentThread;
    if (previous->state == ThreadState::Running) {
        previous->state = ThreadState::Ready;
    }

    next->state = ThreadState::Running;
    next->run_count++;
    g_CurrentThread = next;
    DeliverSignals(*next);

    SchedulerContextSwitch(&previous->context, &next->context);
}

void KernelSchedulerRunSelfTest() {
    const uint64_t demo_process = KernelCreateProcess("scheduler-demo");
    if (demo_process == 0) {
        KernelPanic("Scheduler failed to create process");
    }

    if (KernelCreateThread(demo_process, "worker-a", DemoThreadMain, &g_DemoThreadA) == 0 ||
        KernelCreateThread(demo_process, "worker-b", DemoThreadMain, &g_DemoThreadB) == 0) {
        KernelPanic("Scheduler failed to create thread");
    }

    if (!KernelSetThreadPriority(g_Threads[1].id, 10) ||
        !KernelSetThreadPriority(g_Threads[2].id, 12) ||
        !KernelSendSignal(g_Threads[1].id, 1)) {
        KernelPanic("Scheduler priority/signal self-test failed");
    }

    if (!KernelIpcSend(demo_process, 0xC0FFEE)) {
        KernelPanic("Scheduler IPC send self-test failed");
    }

    uint64_t ipc_value = 0;
    if (!KernelIpcReceive(demo_process, ipc_value) || ipc_value != 0xC0FFEE) {
        KernelPanic("Scheduler IPC receive self-test failed");
    }

    const uint64_t mutex_id = KernelMutexCreate();
    if (mutex_id == 0 || !KernelMutexLock(mutex_id) || !KernelMutexUnlock(mutex_id)) {
        KernelPanic("Scheduler mutex self-test failed");
    }

    g_Threads[2].state = ThreadState::Sleeping;
    g_Threads[2].wake_tick = g_SchedulerTick + 1;

    while (RunnableWorkerCount() > 0) {
        KernelSchedulerYield();
    }

    KernelLog(LogLevel::Info, "Round-robin scheduler self-test complete");
}

void PrintSchedulerInfo() {
    KernelLog(LogLevel::Info, "Processes, threads, context switch, and round-robin online");
    KernelLog(LogLevel::Info, "Priorities, sleeping threads, signals, IPC, and mutexes online");
}
