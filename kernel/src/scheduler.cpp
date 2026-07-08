#include "scheduler.hpp"
#include "kernel.hpp"

extern "C" void SchedulerContextSwitch(void* previous_context, void* next_context);

namespace {

static constexpr uint64_t kMaxProcesses = 8;
static constexpr uint64_t kMaxThreads = 16;
static constexpr uint64_t kThreadStackSize = 8192;
static constexpr uint64_t kKernelProcessId = 1;

enum class ThreadState : uint8_t {
    Empty,
    Ready,
    Running,
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
};

Process g_Processes[kMaxProcesses];
Thread g_Threads[kMaxThreads];
Thread* g_CurrentThread = nullptr;
uint64_t g_NextProcessId = kKernelProcessId;
uint64_t g_NextThreadId = 1;

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

uint64_t ThreadIndex(const Thread* thread) {
    return static_cast<uint64_t>(thread - g_Threads);
}

Thread* FindNextRunnable() {
    if (!g_CurrentThread) {
        return nullptr;
    }

    const uint64_t start = ThreadIndex(g_CurrentThread);
    for (uint64_t offset = 1; offset <= kMaxThreads; offset++) {
        Thread& candidate = g_Threads[(start + offset) % kMaxThreads];
        if (candidate.state == ThreadState::Ready) {
            return &candidate;
        }
    }

    return nullptr;
}

uint64_t RunnableWorkerCount() {
    uint64_t count = 0;

    for (uint64_t i = 0; i < kMaxThreads; i++) {
        const Thread& thread = g_Threads[i];
        if (&thread != g_CurrentThread &&
            thread.state != ThreadState::Empty &&
            thread.state != ThreadState::Finished) {
            count++;
        }
    }

    return count;
}

void ThreadTrampoline() {
    Thread* thread = g_CurrentThread;
    if (!thread || !thread->entry) {
        KernelPanic("Scheduler entered invalid thread");
    }

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

    g_Processes[0] = {
        g_NextProcessId++,
        "kernel",
        true,
    };

    g_Threads[0].id = g_NextThreadId++;
    g_Threads[0].process_id = kKernelProcessId;
    g_Threads[0].name = "bootstrap";
    g_Threads[0].state = ThreadState::Running;
    g_CurrentThread = &g_Threads[0];

    KernelLog(LogLevel::Info, "Scheduler bootstrap thread registered");
    return true;
}

uint64_t KernelCreateProcess(const char* name) {
    for (uint64_t i = 0; i < kMaxProcesses; i++) {
        if (!g_Processes[i].active) {
            g_Processes[i] = {
                g_NextProcessId++,
                name,
                true,
            };
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
    PrepareInitialContext(*thread);
    return thread->id;
}

void KernelSchedulerYield() {
    Thread* next = FindNextRunnable();
    if (!next || next == g_CurrentThread) {
        return;
    }

    Thread* previous = g_CurrentThread;
    if (previous->state == ThreadState::Running) {
        previous->state = ThreadState::Ready;
    }

    next->state = ThreadState::Running;
    next->run_count++;
    g_CurrentThread = next;

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

    while (RunnableWorkerCount() > 0) {
        KernelSchedulerYield();
    }

    KernelLog(LogLevel::Info, "Round-robin scheduler self-test complete");
}

void PrintSchedulerInfo() {
    KernelLog(LogLevel::Info, "Processes, threads, context switch, and round-robin online");
}

