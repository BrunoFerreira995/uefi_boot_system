#pragma once

#include <stdint.h>

struct FileSystemStatus {
    bool vfs_ready;
    bool fat32_supported;
    bool ext2_supported;
    bool file_permissions_ready;
    bool symbolic_links_ready;
    bool mount_manager_ready;
    bool ramfs_ready;
    bool tmpfs_ready;
    bool procfs_ready;
    bool devfs_ready;
    bool sysfs_ready;
    bool initramfs_ready;
    uint32_t registered_driver_count;
    uint32_t mounted_filesystem_count;
    uint32_t ramfs_node_count;
    uint32_t pseudo_node_count;
    uint32_t initramfs_file_count;
};

bool KernelFileSystemInit();
const FileSystemStatus& KernelFileSystemStatus();
void PrintFileSystemInfo();
