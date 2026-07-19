#include "security.hpp"

#include "kernel.hpp"

namespace {

enum class SecurityRole : uint8_t {
    Guest,
    User,
    Admin,
};

enum Permission : uint16_t {
    Read = 0x01,
    Write = 0x02,
    Execute = 0x04,
    Network = 0x08,
    Admin = 0x10,
};

enum VmProtection : uint8_t {
    UserReadable = 0x01,
    UserWritable = 0x02,
    UserExecutable = 0x04,
    KernelOnly = 0x08,
    NoExecute = 0x10,
};

struct SecurityUser {
    uint32_t uid;
    const char* name;
    SecurityRole role;
    uint16_t permissions;
    bool active;
};

struct AccessPolicy {
    const char* resource;
    uint32_t owner_uid;
    uint16_t owner_permissions;
    uint16_t other_permissions;
    bool active;
};

struct ProcessIsolationDomain {
    uint64_t process_id;
    uint32_t uid;
    uint64_t address_space_id;
    bool ipc_restricted;
    bool active;
};

struct VmRegionPolicy {
    uint64_t base;
    uint64_t size;
    uint8_t protection;
    bool active;
};

struct RandomGenerator {
    uint64_t state;
    bool seeded;
};

struct StackCanary {
    uint64_t value;
    bool active;
};

static constexpr uint32_t kMaxUsers = 8;
static constexpr uint32_t kMaxPolicies = 8;
static constexpr uint32_t kMaxDomains = 8;
static constexpr uint32_t kMaxVmRegions = 8;
static constexpr uint64_t kUserAslrBase = 0x400000;
static constexpr uint64_t kKernelAslrBase = 0xFFFFFFFF80000000ull;
static constexpr uint64_t kAslrPageMask = 0xFFFFFull;
static constexpr uint64_t kStackCanaryTerminator = 0x00FF0A0000000000ull;

SecurityStatus g_Status {};
SecurityUser g_Users[kMaxUsers];
AccessPolicy g_Policies[kMaxPolicies];
ProcessIsolationDomain g_Domains[kMaxDomains];
VmRegionPolicy g_VmRegions[kMaxVmRegions];
RandomGenerator g_Random {};
StackCanary g_StackCanary {};

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

void ResetSecurityStatus() {
    g_Status.users_ready = false;
    g_Status.permissions_ready = false;
    g_Status.access_control_ready = false;
    g_Status.process_isolation_ready = false;
    g_Status.virtual_memory_protection_ready = false;
    g_Status.aslr_ready = false;
    g_Status.stack_canaries_ready = false;
    g_Status.nx_pages_ready = false;
    g_Status.secure_random_ready = false;
    g_Status.kaslr_ready = false;
    g_Status.user_count = 0;
    g_Status.policy_count = 0;

    for (uint32_t i = 0; i < kMaxUsers; i++) {
        g_Users[i].uid = 0;
        g_Users[i].name = nullptr;
        g_Users[i].role = SecurityRole::Guest;
        g_Users[i].permissions = 0;
        g_Users[i].active = false;
    }

    for (uint32_t i = 0; i < kMaxPolicies; i++) {
        g_Policies[i].resource = nullptr;
        g_Policies[i].owner_uid = 0;
        g_Policies[i].owner_permissions = 0;
        g_Policies[i].other_permissions = 0;
        g_Policies[i].active = false;
    }

    for (uint32_t i = 0; i < kMaxDomains; i++) {
        g_Domains[i].process_id = 0;
        g_Domains[i].uid = 0;
        g_Domains[i].address_space_id = 0;
        g_Domains[i].ipc_restricted = false;
        g_Domains[i].active = false;
    }

    for (uint32_t i = 0; i < kMaxVmRegions; i++) {
        g_VmRegions[i].base = 0;
        g_VmRegions[i].size = 0;
        g_VmRegions[i].protection = 0;
        g_VmRegions[i].active = false;
    }

    g_Random.state = 0;
    g_Random.seeded = false;
    g_StackCanary.value = 0;
    g_StackCanary.active = false;
}

SecurityUser* CreateUser(uint32_t uid, const char* name, SecurityRole role, uint16_t permissions) {
    if (uid == 0 || !name || g_Status.user_count >= kMaxUsers) {
        return nullptr;
    }

    SecurityUser& user = g_Users[g_Status.user_count];
    user.uid = uid;
    user.name = name;
    user.role = role;
    user.permissions = permissions;
    user.active = true;
    g_Status.user_count++;
    return &user;
}

const SecurityUser* FindUser(uint32_t uid) {
    for (uint32_t i = 0; i < g_Status.user_count; i++) {
        if (g_Users[i].active && g_Users[i].uid == uid) {
            return &g_Users[i];
        }
    }
    return nullptr;
}

bool HasUserPermission(const SecurityUser& user, uint16_t permission) {
    return (user.permissions & permission) == permission;
}

AccessPolicy* AddAccessPolicy(const char* resource,
                              uint32_t owner_uid,
                              uint16_t owner_permissions,
                              uint16_t other_permissions) {
    if (!resource || !FindUser(owner_uid) || g_Status.policy_count >= kMaxPolicies) {
        return nullptr;
    }

    AccessPolicy& policy = g_Policies[g_Status.policy_count];
    policy.resource = resource;
    policy.owner_uid = owner_uid;
    policy.owner_permissions = owner_permissions;
    policy.other_permissions = other_permissions;
    policy.active = true;
    g_Status.policy_count++;
    return &policy;
}

const AccessPolicy* FindPolicy(const char* resource) {
    for (uint32_t i = 0; i < g_Status.policy_count; i++) {
        if (g_Policies[i].active && StringEquals(g_Policies[i].resource, resource)) {
            return &g_Policies[i];
        }
    }
    return nullptr;
}

bool CheckAccess(uint32_t uid, const char* resource, uint16_t permission) {
    const SecurityUser* user = FindUser(uid);
    const AccessPolicy* policy = FindPolicy(resource);
    if (!user || !policy) {
        return false;
    }
    if (HasUserPermission(*user, Admin)) {
        return true;
    }

    const uint16_t allowed = policy->owner_uid == uid
        ? policy->owner_permissions
        : policy->other_permissions;
    return (allowed & permission) == permission;
}

ProcessIsolationDomain* AddProcessIsolationDomain(uint64_t process_id,
                                                  uint32_t uid,
                                                  uint64_t address_space_id,
                                                  bool ipc_restricted) {
    if (process_id == 0 || address_space_id == 0 || !FindUser(uid)) {
        return nullptr;
    }

    for (uint32_t i = 0; i < kMaxDomains; i++) {
        if (!g_Domains[i].active) {
            g_Domains[i].process_id = process_id;
            g_Domains[i].uid = uid;
            g_Domains[i].address_space_id = address_space_id;
            g_Domains[i].ipc_restricted = ipc_restricted;
            g_Domains[i].active = true;
            return &g_Domains[i];
        }
    }
    return nullptr;
}

bool ProcessesAreIsolated(uint64_t lhs_process_id, uint64_t rhs_process_id) {
    const ProcessIsolationDomain* lhs = nullptr;
    const ProcessIsolationDomain* rhs = nullptr;
    for (uint32_t i = 0; i < kMaxDomains; i++) {
        if (g_Domains[i].active && g_Domains[i].process_id == lhs_process_id) {
            lhs = &g_Domains[i];
        }
        if (g_Domains[i].active && g_Domains[i].process_id == rhs_process_id) {
            rhs = &g_Domains[i];
        }
    }

    return lhs && rhs &&
        lhs->address_space_id != rhs->address_space_id &&
        lhs->ipc_restricted &&
        rhs->ipc_restricted;
}

VmRegionPolicy* AddVmRegion(uint64_t base, uint64_t size, uint8_t protection) {
    if (base == 0 || size == 0) {
        return nullptr;
    }

    for (uint32_t i = 0; i < kMaxVmRegions; i++) {
        if (!g_VmRegions[i].active) {
            g_VmRegions[i].base = base;
            g_VmRegions[i].size = size;
            g_VmRegions[i].protection = protection;
            g_VmRegions[i].active = true;
            return &g_VmRegions[i];
        }
    }
    return nullptr;
}

bool VmAccessAllowed(const VmRegionPolicy& region, bool write, bool execute, bool kernel_mode) {
    if ((region.protection & KernelOnly) != 0 && !kernel_mode) {
        return false;
    }
    if (write && (region.protection & UserWritable) == 0) {
        return false;
    }
    if (execute && ((region.protection & UserExecutable) == 0 || (region.protection & NoExecute) != 0)) {
        return false;
    }
    return true;
}

bool VmRegionHasNx(const VmRegionPolicy& region) {
    return (region.protection & NoExecute) != 0;
}

uint64_t MixSecurityEntropy(uint64_t value) {
    value ^= value >> 33;
    value *= 0xFF51AFD7ED558CCDull;
    value ^= value >> 33;
    value *= 0xC4CEB9FE1A85EC53ull;
    value ^= value >> 33;
    return value;
}

bool SeedSecureRandom(uint64_t seed) {
    if (seed == 0) {
        return false;
    }

    g_Random.state = MixSecurityEntropy(seed);
    g_Random.seeded = g_Random.state != 0;
    return g_Random.seeded;
}

uint64_t SecureRandom64() {
    if (!g_Random.seeded) {
        return 0;
    }

    uint64_t x = g_Random.state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    g_Random.state = x;
    return x;
}

uint64_t GenerateAslrOffset(uint64_t image_id, uint64_t salt) {
    const uint64_t mixed = MixSecurityEntropy(image_id ^ salt);
    return (mixed & kAslrPageMask) << 12;
}

uint64_t ApplyUserAslr(uint64_t image_id, uint64_t salt) {
    return kUserAslrBase + GenerateAslrOffset(image_id, salt);
}

uint64_t ApplyKernelAslr(uint64_t boot_seed) {
    return kKernelAslrBase + GenerateAslrOffset(0xC0DEF00Dull, boot_seed);
}

uint64_t GenerateStackCanary(uint64_t thread_id) {
    const uint64_t random = SecureRandom64();
    if (random == 0 || thread_id == 0) {
        return 0;
    }

    g_StackCanary.value = (random ^ thread_id) | kStackCanaryTerminator;
    g_StackCanary.active = true;
    return g_StackCanary.value;
}

bool CheckStackCanary(uint64_t expected, uint64_t observed) {
    return g_StackCanary.active && expected == observed && observed == g_StackCanary.value;
}

bool RunUsersSelfTest() {
    return CreateUser(1, "root", SecurityRole::Admin, Read | Write | Execute | Network | Admin) &&
        CreateUser(1000, "shell", SecurityRole::User, Read | Write | Execute | Network) &&
        CreateUser(65534, "guest", SecurityRole::Guest, Read) &&
        g_Status.user_count == 3 &&
        FindUser(1000) != nullptr;
}

bool RunPermissionsSelfTest() {
    const SecurityUser* shell = FindUser(1000);
    const SecurityUser* guest = FindUser(65534);
    return shell &&
        guest &&
        HasUserPermission(*shell, Execute) &&
        HasUserPermission(*shell, Network) &&
        HasUserPermission(*guest, Read) &&
        !HasUserPermission(*guest, Write);
}

bool RunAccessControlSelfTest() {
    return AddAccessPolicy("/tmp/readme", 1000, Read | Write, Read) &&
        AddAccessPolicy("/boot/kernel.elf", 1, Read | Execute, 0) &&
        CheckAccess(1000, "/tmp/readme", Write) &&
        CheckAccess(65534, "/tmp/readme", Read) &&
        !CheckAccess(65534, "/tmp/readme", Write) &&
        CheckAccess(1, "/boot/kernel.elf", Write);
}

bool RunProcessIsolationSelfTest() {
    return AddProcessIsolationDomain(10, 1000, 0x1000, true) &&
        AddProcessIsolationDomain(11, 65534, 0x2000, true) &&
        ProcessesAreIsolated(10, 11);
}

bool RunVirtualMemoryProtectionSelfTest() {
    VmRegionPolicy* user_text = AddVmRegion(0x400000, 0x2000, UserReadable | UserExecutable);
    VmRegionPolicy* user_data = AddVmRegion(0x600000, 0x2000, UserReadable | UserWritable | NoExecute);
    VmRegionPolicy* kernel = AddVmRegion(0xFFFFFFFF80000000ull, 0x200000, KernelOnly | NoExecute);

    return user_text &&
        user_data &&
        kernel &&
        VmAccessAllowed(*user_text, false, true, false) &&
        !VmAccessAllowed(*user_text, true, false, false) &&
        VmAccessAllowed(*user_data, true, false, false) &&
        !VmAccessAllowed(*user_data, false, true, false) &&
        !VmAccessAllowed(*kernel, false, false, false) &&
        VmAccessAllowed(*kernel, false, false, true);
}

bool RunAslrSelfTest() {
    const uint64_t first = ApplyUserAslr(0x1000, 0xABCD);
    const uint64_t second = ApplyUserAslr(0x1000, 0xBCDE);
    return first >= kUserAslrBase &&
        second >= kUserAslrBase &&
        first != second &&
        (first & 0xFFF) == 0 &&
        (second & 0xFFF) == 0;
}

bool RunStackCanarySelfTest() {
    if (!SeedSecureRandom(0x5152535455565758ull)) {
        return false;
    }

    const uint64_t canary = GenerateStackCanary(42);
    return canary != 0 &&
        CheckStackCanary(canary, canary) &&
        !CheckStackCanary(canary, canary ^ 1);
}

bool RunNxPagesSelfTest() {
    VmRegionPolicy* stack = AddVmRegion(0x700000, 0x4000, UserReadable | UserWritable | NoExecute);
    VmRegionPolicy* code = AddVmRegion(0x800000, 0x4000, UserReadable | UserExecutable);
    return stack &&
        code &&
        VmRegionHasNx(*stack) &&
        !VmAccessAllowed(*stack, false, true, false) &&
        !VmRegionHasNx(*code) &&
        VmAccessAllowed(*code, false, true, false);
}

bool RunSecureRandomSelfTest() {
    if (!SeedSecureRandom(0xA5A55A5ADEADBEEFull)) {
        return false;
    }

    const uint64_t first = SecureRandom64();
    const uint64_t second = SecureRandom64();
    return first != 0 && second != 0 && first != second;
}

bool RunKaslrSelfTest() {
    const uint64_t first = ApplyKernelAslr(0x12345678);
    const uint64_t second = ApplyKernelAslr(0x87654321);
    return first >= kKernelAslrBase &&
        second >= kKernelAslrBase &&
        first != second &&
        (first & 0xFFF) == 0 &&
        (second & 0xFFF) == 0;
}

} // namespace

bool KernelSecurityInit() {
    ResetSecurityStatus();

    g_Status.users_ready = RunUsersSelfTest();
    g_Status.permissions_ready = g_Status.users_ready && RunPermissionsSelfTest();
    g_Status.access_control_ready = g_Status.permissions_ready && RunAccessControlSelfTest();
    g_Status.process_isolation_ready = g_Status.access_control_ready && RunProcessIsolationSelfTest();
    g_Status.virtual_memory_protection_ready =
        g_Status.process_isolation_ready && RunVirtualMemoryProtectionSelfTest();
    g_Status.aslr_ready = g_Status.virtual_memory_protection_ready && RunAslrSelfTest();
    g_Status.stack_canaries_ready = g_Status.aslr_ready && RunStackCanarySelfTest();
    g_Status.nx_pages_ready = g_Status.stack_canaries_ready && RunNxPagesSelfTest();
    g_Status.secure_random_ready = g_Status.nx_pages_ready && RunSecureRandomSelfTest();
    g_Status.kaslr_ready = g_Status.secure_random_ready && RunKaslrSelfTest();

    KernelLog(LogLevel::Info, "Phase 15 security initialized");
    return g_Status.users_ready &&
        g_Status.permissions_ready &&
        g_Status.access_control_ready &&
        g_Status.process_isolation_ready &&
        g_Status.virtual_memory_protection_ready &&
        g_Status.aslr_ready &&
        g_Status.stack_canaries_ready &&
        g_Status.nx_pages_ready &&
        g_Status.secure_random_ready &&
        g_Status.kaslr_ready;
}

const SecurityStatus& KernelSecurityStatus() {
    return g_Status;
}

void PrintSecurityInfo() {
    KernelLog(g_Status.users_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.users_ready ? "Security users ready" : "Security users unavailable");
    KernelLog(g_Status.permissions_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.permissions_ready ? "Security permissions ready" : "Security permissions unavailable");
    KernelLog(g_Status.access_control_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.access_control_ready ? "Access control ready" : "Access control unavailable");
    KernelLog(g_Status.process_isolation_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.process_isolation_ready ? "Process isolation policy ready" : "Process isolation unavailable");
    KernelLog(g_Status.virtual_memory_protection_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.virtual_memory_protection_ready ? "Virtual memory protection policy ready" : "Virtual memory protection unavailable");
    KernelLog(g_Status.aslr_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.aslr_ready ? "ASLR offsets ready" : "ASLR offsets unavailable");
    KernelLog(g_Status.stack_canaries_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.stack_canaries_ready ? "Stack canaries ready" : "Stack canaries unavailable");
    KernelLog(g_Status.nx_pages_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.nx_pages_ready ? "NX page policy ready" : "NX page policy unavailable");
    KernelLog(g_Status.secure_random_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.secure_random_ready ? "Secure random generator ready" : "Secure random generator unavailable");
    KernelLog(g_Status.kaslr_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.kaslr_ready ? "Kernel ASLR offsets ready" : "Kernel ASLR offsets unavailable");
}
