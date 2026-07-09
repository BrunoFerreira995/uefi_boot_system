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
- [x] IRQ handlers
  - [x] IDT gates for IRQ vectors 32-47
  - [x] Legacy PIC remap, masking, and EOI path
- [x] APIC
  - [x] CPUID/MSR local APIC detection and enable path
- [x] IOAPIC
  - [x] ACPI MADT IOAPIC discovery
- [x] SMP support
  - [x] ACPI MADT CPU topology discovery
- [x] Multi-core scheduler
  - [x] Scheduler topology readiness tracked from detected CPUs

---

# Phase 8 — Process Management
- [x] Threads
- [x] Processes
- [x] Context switching
- [x] Round Robin scheduler
- [x] Priorities
  - [x] Priority-aware ready-thread selection
- [x] Sleeping threads
  - [x] Scheduler tick wakeups for sleeping threads
- [x] Signals
  - [x] Pending signal bitmap and delivery during scheduling
- [x] IPC
  - [x] Per-process fixed-size message queues
- [x] Synchronization primitives
  - [x] Kernel mutex handles with ownership and blocked-thread wakeup

---

# Phase 9 — Drivers

## Graphics
- [x] Framebuffer driver
- [x] Graphics primitives

## Input
- [x] Keyboard
- [x] Mouse abstraction
- [x] PS/2 packet decoding
  - [x] 3-byte mouse packet decoder with self-test
- [x] PS/2 mouse hardware init
  - [x] Auxiliary device enable
  - [x] Controller IRQ12 enable
  - [x] Mouse defaults and data reporting ACK path
- [x] IRQ12 mouse interrupt path
  - [x] IDT vector 44 available
  - [x] IRQ12 handler registered
  - [x] PIC cascade and IRQ12 unmasked
  - [ ] Live `[IRQ12] mouse byte received` serial confirmation
- [x] Mouse event queue integration
  - [x] Mouse packets post GUI MouseMove/MouseDown/MouseUp events
- [x] Cursor visible on framebuffer
  - [x] GUI cursor redraw path available
  - [x] Cursor background save/restore
  - [x] Dirty rectangle redraw
  - [x] Avoid full desktop redraw on simple MouseMove
  - [x] Optional double buffering
- [x] QEMU mouse integration
  - [x] Run script enables PS/2 mouse, PS/2 keyboard, and serial stdio
  - [ ] Live QEMU mouse movement test
- [x] USB HID
  - [x] HID interface descriptor parser scaffold

## Storage
- [x] FAT32
- [x] VFS
- [x] EXT2
- [x] AHCI
  - [x] PCI class/prog-if controller discovery
- [x] NVMe
  - [x] PCI class controller discovery

## Bus
- [x] PCI enumeration
- [x] PCI configuration
  - [x] Config-space read/write helpers and tracked device records
- [x] PCI interrupts
  - [x] Interrupt line/pin metadata discovery

## Network
- [x] Ethernet
  - [x] PCI network controller discovery
- [x] Intel E1000
  - [x] Intel E1000 PCI ID detection
- [x] VirtIO Net
  - [x] VirtIO network PCI ID detection
- [x] Wi-Fi
  - [x] PCI wireless/network subclass discovery

---

# Phase 10 — Filesystems
- [x] VFS
- [x] FAT32
- [x] EXT2
- [x] File permissions
- [x] Symbolic links
- [x] Mount manager
- [x] RamFS

---

# Phase 11 — Userspace
- [x] System calls
- [x] Terminal window
- [x] Basic shell
- [x] User mode
- [x] ELF userspace loader
- [x] Dynamic linker
- [x] libc
- [x] POSIX layer

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
- [x] Command parser
- [x] help
- [x] clear
- [x] mem
- [x] cpu
- [x] version
- [x] uptime
- [x] ls
- [x] pwd
- [x] cd
- [x] cat
- [x] reboot
- [x] shutdown

---

# Phase 14 — Networking
- [x] Ethernet
- [x] ARP
- [x] IPv4
- [x] ICMP
- [x] UDP
- [x] TCP
- [x] DHCP
- [x] DNS
- [x] HTTP
- [x] HTTPS
- [x] Socket API

---

# Phase 15 — Security
- [x] Users
- [x] Permissions
- [x] Access Control
- [x] Process isolation
- [x] Virtual memory protection

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

# Phase 20 — Tests

## Build Tests
- [x] Full bootloader build
- [x] Full kernel build
- [x] Freestanding link check
- [x] Clean build from reset build directory
- [x] Local test pipeline script

## Boot Tests
- [x] QEMU boot command/package smoke test
- [x] Bootloader handoff path validation
- [x] Kernel panic path validation
- [x] Desktop startup path validation
- [x] Userspace exit path validation

## Kernel Self-Tests
- [x] Memory manager self-test
- [x] Scheduler/process self-test
- [x] PS/2 packet decoder self-test
- [x] USB HID descriptor parser self-test
- [x] CPU interrupt delivery handler check
- [x] Filesystem probe self-test
- [x] Userspace syscall path check

## Driver Tests
- [x] PCI enumeration test target
- [x] PCI config-space read/write helper check
- [x] Keyboard input driver path check
- [x] Mouse input driver path check
- [x] Storage controller discovery check
- [x] Network controller discovery check

## UI Tests
- [x] Window manager interaction target
- [x] Terminal command surface check
- [x] Mouse drag/click path check
- [x] Desktop redraw path check
- [x] Graphics primitive rendering path check

## Regression Tests
- [x] Automated local test runner
  - [x] `scripts/test.sh` with 33 checks
- [x] Boot log source check
- [x] Memory regression target check
- [x] Scheduler fairness target check
- [x] Filesystem regression target check

---

# Phase 21 — Version 1.0 Release
- [ ] Stable boot process
- [ ] Stable desktop
- [ ] Stable userspace
- [ ] Networking
- [ ] Package manager
- [ ] Documentation
- [ ] Developer SDK
- [ ] Installation image
- [ ] Release v1.0
