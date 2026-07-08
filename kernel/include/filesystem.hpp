#pragma once

#include <stdint.h>

struct FileSystemStatus {
    bool vfs_ready;
    bool fat32_supported;
    bool ext2_supported;
    uint32_t registered_driver_count;
    uint32_t mounted_filesystem_count;
};

bool KernelFileSystemInit();
const FileSystemStatus& KernelFileSystemStatus();
void PrintFileSystemInfo();
