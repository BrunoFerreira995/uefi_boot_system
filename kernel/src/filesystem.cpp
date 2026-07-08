#include "filesystem.hpp"

#include "kernel.hpp"

namespace {

enum class FileSystemType {
    Unknown,
    Fat32,
    Ext2,
};

struct VfsMount {
    const char* path;
    FileSystemType type;
    bool active;
};

struct FsDriver {
    const char* name;
    FileSystemType type;
    bool (*Probe)(const uint8_t* data, uint64_t size);
};

static constexpr uint32_t kMaxFsDrivers = 4;
static constexpr uint32_t kMaxMounts = 8;
static constexpr uint16_t kFatBootSignature = 0xAA55;
static constexpr uint16_t kExt2Magic = 0xEF53;

FsDriver g_Drivers[kMaxFsDrivers];
VfsMount g_Mounts[kMaxMounts];
FileSystemStatus g_Status {};

uint16_t ReadLe16(const uint8_t* data, uint64_t offset) {
    return static_cast<uint16_t>(data[offset]) |
        static_cast<uint16_t>(static_cast<uint16_t>(data[offset + 1]) << 8);
}

uint32_t ReadLe32(const uint8_t* data, uint64_t offset) {
    return static_cast<uint32_t>(data[offset]) |
        (static_cast<uint32_t>(data[offset + 1]) << 8) |
        (static_cast<uint32_t>(data[offset + 2]) << 16) |
        (static_cast<uint32_t>(data[offset + 3]) << 24);
}

bool Fat32Probe(const uint8_t* data, uint64_t size) {
    if (!data || size < 512) {
        return false;
    }

    const uint16_t signature = ReadLe16(data, 510);
    const uint16_t bytes_per_sector = ReadLe16(data, 11);
    const uint8_t sectors_per_cluster = data[13];
    const uint16_t reserved_sectors = ReadLe16(data, 14);
    const uint8_t fat_count = data[16];
    const uint16_t root_entry_count = ReadLe16(data, 17);
    const uint16_t fat16_size = ReadLe16(data, 22);
    const uint32_t fat32_size = ReadLe32(data, 36);
    const uint32_t root_cluster = ReadLe32(data, 44);

    return signature == kFatBootSignature &&
        bytes_per_sector >= 512 &&
        sectors_per_cluster != 0 &&
        reserved_sectors != 0 &&
        fat_count != 0 &&
        root_entry_count == 0 &&
        fat16_size == 0 &&
        fat32_size != 0 &&
        root_cluster >= 2;
}

bool Ext2Probe(const uint8_t* data, uint64_t size) {
    if (!data || size < 2048) {
        return false;
    }

    const uint64_t superblock = 1024;
    const uint32_t inode_count = ReadLe32(data, superblock + 0);
    const uint32_t block_count = ReadLe32(data, superblock + 4);
    const uint16_t magic = ReadLe16(data, superblock + 56);

    return magic == kExt2Magic && inode_count != 0 && block_count != 0;
}

void ResetFileSystemStatus() {
    g_Status.vfs_ready = false;
    g_Status.fat32_supported = false;
    g_Status.ext2_supported = false;
    g_Status.registered_driver_count = 0;
    g_Status.mounted_filesystem_count = 0;

    for (uint32_t i = 0; i < kMaxFsDrivers; i++) {
        g_Drivers[i].name = nullptr;
        g_Drivers[i].type = FileSystemType::Unknown;
        g_Drivers[i].Probe = nullptr;
    }

    for (uint32_t i = 0; i < kMaxMounts; i++) {
        g_Mounts[i].path = nullptr;
        g_Mounts[i].type = FileSystemType::Unknown;
        g_Mounts[i].active = false;
    }
}

bool VfsRegisterDriver(const char* name, FileSystemType type, bool (*probe)(const uint8_t*, uint64_t)) {
    if (!name || !probe || g_Status.registered_driver_count >= kMaxFsDrivers) {
        return false;
    }

    FsDriver& driver = g_Drivers[g_Status.registered_driver_count];
    driver.name = name;
    driver.type = type;
    driver.Probe = probe;
    g_Status.registered_driver_count++;
    return true;
}

bool VfsMountPath(const char* path, FileSystemType type) {
    if (!path || type == FileSystemType::Unknown || g_Status.mounted_filesystem_count >= kMaxMounts) {
        return false;
    }

    VfsMount& mount = g_Mounts[g_Status.mounted_filesystem_count];
    mount.path = path;
    mount.type = type;
    mount.active = true;
    g_Status.mounted_filesystem_count++;
    return true;
}

void WriteLe16(uint8_t* data, uint64_t offset, uint16_t value) {
    data[offset] = static_cast<uint8_t>(value & 0xFF);
    data[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

void WriteLe32(uint8_t* data, uint64_t offset, uint32_t value) {
    data[offset] = static_cast<uint8_t>(value & 0xFF);
    data[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    data[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    data[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

bool RunFat32ProbeSelfTest() {
    uint8_t sector[512];
    for (uint32_t i = 0; i < sizeof(sector); i++) {
        sector[i] = 0;
    }

    WriteLe16(sector, 11, 512);
    sector[13] = 8;
    WriteLe16(sector, 14, 32);
    sector[16] = 2;
    WriteLe16(sector, 17, 0);
    WriteLe16(sector, 22, 0);
    WriteLe32(sector, 36, 128);
    WriteLe32(sector, 44, 2);
    WriteLe16(sector, 510, kFatBootSignature);

    return Fat32Probe(sector, sizeof(sector));
}

bool RunExt2ProbeSelfTest() {
    uint8_t image[2048];
    for (uint32_t i = 0; i < sizeof(image); i++) {
        image[i] = 0;
    }

    const uint64_t superblock = 1024;
    WriteLe32(image, superblock + 0, 128);
    WriteLe32(image, superblock + 4, 1024);
    WriteLe16(image, superblock + 56, kExt2Magic);

    return Ext2Probe(image, sizeof(image));
}

const char* TypeName(FileSystemType type) {
    switch (type) {
        case FileSystemType::Fat32:
            return "FAT32";
        case FileSystemType::Ext2:
            return "EXT2";
        case FileSystemType::Unknown:
            return "unknown";
    }

    return "unknown";
}

} // namespace

bool KernelFileSystemInit() {
    ResetFileSystemStatus();

    g_Status.vfs_ready =
        VfsRegisterDriver("fat32", FileSystemType::Fat32, Fat32Probe) &&
        VfsRegisterDriver("ext2", FileSystemType::Ext2, Ext2Probe);

    g_Status.fat32_supported = RunFat32ProbeSelfTest();
    g_Status.ext2_supported = RunExt2ProbeSelfTest();

    if (g_Status.fat32_supported) {
        VfsMountPath("/boot", FileSystemType::Fat32);
    }

    if (g_Status.ext2_supported) {
        VfsMountPath("/", FileSystemType::Ext2);
    }

    KernelLog(LogLevel::Info, "Phase 8 filesystem initialized");
    return g_Status.vfs_ready && g_Status.fat32_supported && g_Status.ext2_supported;
}

const FileSystemStatus& KernelFileSystemStatus() {
    return g_Status;
}

void PrintFileSystemInfo() {
    KernelLog(g_Status.vfs_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.vfs_ready ? "VFS registry ready" : "VFS registry unavailable");
    KernelLog(g_Status.fat32_supported ? LogLevel::Info : LogLevel::Warn,
        g_Status.fat32_supported ? "FAT32 recognizer ready" : "FAT32 recognizer failed");
    KernelLog(g_Status.ext2_supported ? LogLevel::Info : LogLevel::Warn,
        g_Status.ext2_supported ? "EXT2 recognizer ready" : "EXT2 recognizer failed");

    for (uint32_t i = 0; i < g_Status.mounted_filesystem_count; i++) {
        if (!g_Mounts[i].active) {
            continue;
        }

        KernelLog(LogLevel::Info, TypeName(g_Mounts[i].type));
    }
}
