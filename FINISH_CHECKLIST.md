# Project Finish Checklist

> Current Status: Graphical UEFI operating system prototype with a custom bootloader, ELF64 kernel, memory management, scheduler, GUI, and early userspace.

---

# Phase 1 — Development Environment
- [x] Install CMake
- [x] Install Clang/LLVM
- [x] Install x86_64-elf-binutils
- [x] Install x86_64-w64-mingw32 cross compiler
- [x] Install QEMU
- [x] Install OVMF / EDK2 firmware
- [x] Configure build scripts
- [x] Configure CMake project

---

# Phase 2 — Build System
- [x] Build bootloader
- [x] Build kernel
- [x] Generate EFI System Partition (ESP)
- [x] Package boot files
- [x] Launch QEMU automatically

---

# Phase 3 — UEFI Bootloader
- [x] Locate EFI System Partition
- [x] Open root filesystem
- [x] Locate BOOTX64.EFI
- [x] Load kernel ELF
- [x] Parse ELF64 executable
- [x] Allocate kernel memory
- [x] Relocate ELF segments
- [x] Build BootInfo structure
- [x] ExitBootServices()
- [x] Jump to kernel

---

# Phase 4 — Boot Information
- [x] Memory Map
- [x] Graphics Output Protocol
- [x] Framebuffer information
- [x] ACPI RSDP
- [x] CPUID
- [x] BootInfo structure

---

# Phase 5 — Kernel Initialization
- [x] Kernel entry point
- [x] Kernel logger
- [x] Panic handler
- [x] Framebuffer console
- [x] Early initialization
- [x] Kernel memory initialization

---

# Phase 6 — Memory Management
- [x] Physical Memory Manager
  - [x] Bitmap-backed page tracking from the UEFI memory map
  - [x] Reservation of low memory, kernel image, framebuffer, and paging structures
- [x] Virtual Memory Manager
- [x] Paging
  - [x] 0-4GiB identity map with 2MiB pages
- [x] Kernel Heap
  - [x] Early bump allocator backed by physical pages
- [x] Page allocator
- [x] Copy-on-write
  - [x] Refcounted physical pages with private copy on write resolution
- [x] Shared memory
  - [x] Refcounted multi-page shared segments
- [x] Slab allocator
  - [x] Fixed-size caches for small kernel objects

---

# Phase 7 — CPU
- [x] GDT
- [x] IDT
- [x] TSS
- [x] Exception handlers
- [ ] IRQ handlers
- [ ] APIC
- [ ] IOAPIC
- [ ] SMP support
- [ ] Multi-core scheduler

---

# Phase 8 — Process Management
- [x] Threads
- [x] Processes
- [x] Context switching
- [x] Round Robin scheduler
- [ ] Priorities
- [ ] Sleeping threads
- [ ] Signals
- [ ] IPC
- [ ] Synchronization primitives

---

# Phase 9 — Drivers

## Graphics
- [x] Framebuffer driver
- [x] Graphics primitives

## Input
- [x] Keyboard
- [x] Mouse abstraction
- [ ] PS/2 packet decoding
- [ ] USB HID

## Storage
- [x] FAT32
- [x] VFS
- [x] EXT2
- [ ] AHCI
- [ ] NVMe

## Bus
- [x] PCI enumeration
- [ ] PCI configuration
- [ ] PCI interrupts

## Network
- [ ] Ethernet
- [ ] Intel E1000
- [ ] VirtIO Net
- [ ] Wi-Fi

---

# Phase 10 — Filesystems
- [x] VFS
- [x] FAT32
- [x] EXT2
- [ ] File permissions
- [ ] Symbolic links
- [ ] Mount manager
- [ ] RamFS

---

# Phase 11 — Userspace
- [x] System calls
- [x] Terminal window
- [x] Basic shell
- [x] User mode
- [ ] ELF userspace loader
- [ ] Dynamic linker
- [ ] libc
- [ ] POSIX layer

---

# Phase 12 — Desktop Environment

## Window Manager
- [x] Window manager
- [x] Compositor
- [x] Z-order
- [x] Drag windows
- [x] Window states
- [x] Close button
- [x] Minimize
- [x] Maximize
- [x] Restore

## Desktop
- [x] Wallpaper
- [x] Desktop icons
- [x] Taskbar
- [x] Window buttons

## Events
- [x] MouseMove
- [x] MouseDown
- [x] MouseUp
- [x] Click
- [x] Hover
- [x] Drag
- [ ] DoubleClick
- [ ] Mouse wheel

## Graphics
- [x] Rectangle API
- [x] Line API
- [x] Text API
- [x] Image placeholder
- [ ] PNG support
- [ ] BMP support
- [ ] TrueType fonts

---

# Phase 13 — Terminal
- [x] Terminal window
- [ ] Command parser
- [ ] help
- [ ] clear
- [ ] mem
- [ ] cpu
- [ ] version
- [ ] uptime
- [ ] ls
- [ ] pwd
- [ ] cd
- [ ] cat
- [ ] reboot
- [ ] shutdown

---

# Phase 14 — Networking
- [ ] Ethernet
- [ ] ARP
- [ ] IPv4
- [ ] ICMP
- [ ] UDP
- [ ] TCP
- [ ] DHCP
- [ ] DNS
- [ ] HTTP
- [ ] HTTPS
- [ ] Socket API

---

# Phase 15 — Security
- [ ] Users
- [ ] Permissions
- [ ] Access Control
- [ ] Process isolation
- [ ] Virtual memory protection

---

# Phase 16 — Applications
- [ ] File Manager
- [ ] Text Editor
- [ ] Image Viewer
- [ ] Calculator
- [ ] Settings
- [ ] Task Manager
- [ ] Package Manager

---

# Phase 17 — Development SDK
- [ ] C Standard Library
- [ ] C++ Runtime
- [ ] Build SDK
- [ ] Documentation
- [ ] Example applications
- [ ] Developer tools

---

# Phase 18 — Compatibility
- [ ] POSIX compatibility
- [ ] ELF executable compatibility
- [ ] SDL2 support
- [ ] SDL3 support
- [ ] OpenGL abstraction
- [ ] Vulkan abstraction

---

# Phase 19 — Performance
- [ ] Hardware acceleration
- [ ] Optimized compositor
- [ ] Multi-core scheduling
- [ ] Profiling tools
- [ ] Benchmark suite

---

# Phase 20 — Version 1.0 Release
- [ ] Stable boot process
- [ ] Stable desktop
- [ ] Stable userspace
- [ ] Networking
- [ ] Package manager
- [ ] Documentation
- [ ] Developer SDK
- [ ] Installation image
- [ ] Release v1.0
