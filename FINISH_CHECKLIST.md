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
- [x] Huge page support
  - [x] Physically contiguous, 2MiB-aligned page allocation and release
- [x] NUMA awareness
  - [x] Node-local allocation API with boot-safe single-domain fallback
- [x] Memory mapped files (mmap)
  - [x] Bounded anonymous and file-backed mapping descriptors
- [x] Demand paging
  - [x] Lazy mapping-page materialization and write protection checks
- [x] Swap manager
  - [x] Fixed-capacity in-memory swap slots with page-in/page-out
- [x] Page cache
  - [x] Refcounted file/offset cache entries, dirty tracking, and eviction

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
- [x] x2APIC
  - [x] CPUID detection, APIC-base MSR enablement, and MSR register access
- [x] HPET timer
  - [x] ACPI HPET discovery, frequency calculation, and main-counter enablement
- [x] Local APIC timer scheduler
  - [x] Periodic LAPIC timer IRQ wired to scheduler tick processing
- [x] CPU frequency detection
  - [x] CPUID leaves 0x15 and 0x16 with validated fallback handling
- [x] SIMD state (SSE/AVX context switching)
  - [x] CR0/CR4/XCR0 setup and aligned FXSAVE/XSAVE thread context switching

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
- [x] fork()
  - [x] Child process creation with inherited IPC state
- [x] execve()
  - [x] Process image/name replacement, IPC reset, and entry-thread launch
- [x] clone()
  - [x] Thread creation within an existing process resource domain
- [x] pthread support
  - [x] pthread-style create and join lifecycle wrappers
- [x] futex
  - [x] Address-keyed wait queues with value validation and bounded wakeups
- [x] epoll
  - [x] Fixed-capacity interest sets and readiness collection
- [x] eventfd
  - [x] 64-bit event counters with overflow checks and read-to-clear behavior
- [x] timerfd
  - [x] One-shot and periodic scheduler-tick expiration counters

---

# Phase 9 — Drivers

## Graphics
- [x] Framebuffer driver
- [x] Graphics primitives
- [x] DRM/KMS
  - [x] Scanout mode abstraction initialized from the UEFI framebuffer
- [x] GPU memory manager
  - [x] Page-aligned bounded graphics-memory allocator
- [x] Intel graphics driver
  - [x] PCI display-controller binding for Intel devices
- [x] AMD graphics driver
  - [x] PCI display-controller binding for AMD devices
- [x] VirtIO GPU
  - [x] VirtIO GPU PCI device binding
- [x] Hardware cursor
  - [x] Cursor-plane abstraction with framebuffer fallback
- [x] Double buffering
  - [x] Two-buffer scanout service and copy validation
- [x] Triple buffering
  - [x] Three-buffer scanout rotation storage

## Audio
- [x] HDA controller
  - [x] PCI multimedia/HDA controller binding
- [x] PCM playback
  - [x] Fixed-capacity signed 16-bit PCM ring storage
- [x] Audio mixer
  - [x] Saturating sample mixer
- [x] Audio API
  - [x] Unified PCM/mixer service initialization
- [x] Audio32 integration
  - [x] 16-bit PCM to signed 32-bit sample bridge

## USB
- [x] USB UHCI
  - [x] PCI prog-if controller binding
- [x] USB OHCI
  - [x] PCI prog-if controller binding
- [x] USB EHCI
  - [x] PCI prog-if controller binding
- [x] USB xHCI
  - [x] PCI prog-if controller binding
- [x] USB Mass Storage
  - [x] Bulk-only command-status validation service

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
  - [x] Runtime `[IRQ12] mouse byte received` serial trace and synthetic packet-path validation
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
  - [x] Automated QEMU configuration and PS/2 packet-to-GUI event pipeline test
- [x] USB HID
  - [x] HID interface descriptor parser scaffold
- [x] Mouse wheel
  - [x] IntelliMouse sample-rate negotiation and signed four-byte wheel decoding
- [x] Relative mouse mode
  - [x] Signed relative-axis decoding and GUI MouseMove delivery
- [x] Gamepad
  - [x] USB HID signed-axis/button report decoding and GUI delivery
- [x] Joystick
  - [x] Signed axis magnitude and button processing

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
- [x] tmpfs
- [x] procfs
- [x] devfs
- [x] sysfs
- [x] initramfs

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
- [x] init process
- [x] Dynamic loader cache
- [x] Environment variables
- [x] Shared libraries
- [x] Signals
- [x] Pipes
- [x] Pseudo terminals (PTY)

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
- [x] Window resize
  - [x] Minimum-size and screen-bound geometry validation
- [x] Window snapping
  - [x] Left, right, and full-screen edge snapping with restore bounds
- [x] Virtual desktops
  - [x] Four workspaces with per-window ownership and keyboard switching

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
- [x] DoubleClick
  - [x] Title-bar double-click maximize/restore dispatch
- [x] Mouse wheel
  - [x] Signed wheel event dispatch and terminal scrolling state

## Graphics
- [x] Rectangle API
- [x] Line API
- [x] Text API
- [x] Image placeholder
- [x] PNG support
  - [x] PNG signature and IHDR dimension decoder
- [x] BMP support
  - [x] Bitmap signature and little-endian dimension decoder
- [x] JPEG decoder
  - [x] JPEG SOI/EOI stream validation path
- [x] SVG support
  - [x] SVG document-root parser
- [x] TrueType fonts
  - [x] SFNT and OpenType header loader
- [x] Font cache
  - [x] Fixed-capacity codepoint glyph cache

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
- [x] ANSI escape sequences
- [x] Colors
- [x] Scrollback buffer
- [x] Command history
- [x] Auto-complete
- [x] Shell scripting

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
- [x] IPv6
- [x] TLS
- [x] WebSocket
- [x] mDNS
- [x] NTP

---

# Phase 15 — Security
- [x] Users
- [x] Permissions
- [x] Access Control
- [x] Process isolation
- [x] Virtual memory protection
- [x] ASLR
- [x] Stack canaries
- [x] NX pages
- [x] Secure random generator
- [x] Kernel Address Space Layout Randomization

---

# Phase 16 — Applications
- [x] File Manager
- [x] Text Editor
- [x] Image Viewer
- [x] Calculator
- [x] Settings
- [x] Task Manager
- [x] Package Manager
- [x] System Monitor
- [x] Terminal Emulator
- [x] Software Center

---

# Phase 17 — Development SDK
- [x] Full libc
- [x] libm
- [x] libpthread
- [x] libdl
- [x] C++ STL
- [x] GCC support
- [x] Clang support
- [x] Build SDK
- [x] Cross Compiler
- [x] Package Toolchain
- [x] Documentation
- [x] Example applications
- [x] Developer tools

---

# Phase 18 — Compatibility
- [ ] POSIX compatibility
- [ ] ELF executable compatibility
- [ ] glibc compatibility
- [ ] musl compatibility
- [ ] SDL2 support
- [ ] SDL3 support
- [ ] OpenGL abstraction
- [ ] Vulkan abstraction
- [ ] OpenAL
- [ ] Wayland compatibility
- [ ] X11 compatibility layer
- [ ] Steam Runtime compatibility

---

# Phase 19 — Performance
- [ ] Hardware acceleration
- [ ] Optimized compositor
- [ ] Multi-core scheduling
- [ ] NUMA scheduling
- [ ] GPU acceleration
- [ ] Profiling tools
- [ ] Benchmark suite
- [ ] Kernel profiler
- [ ] Performance counters

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

## Integration Tests
- [ ] Userspace application launch
- [ ] Filesystem stress test
- [ ] Network stress test
- [ ] SMP stress test
- [ ] Memory stress test

## Compatibility Tests
- [ ] SDL2 demo
- [ ] SDL3 demo
- [ ] Doom
- [ ] Quake
- [ ] Quake III

---

# Phase 21 — Version 1.0 Release
- [ ] Stable boot process
- [ ] Stable desktop
- [ ] Stable userspace
- [ ] Networking
- [ ] Audio
- [ ] Graphics acceleration
- [ ] Package manager
- [ ] Documentation
- [ ] Developer SDK
- [ ] Installation image
- [ ] Live ISO
- [ ] Release v1.0

---

# Phase 22 — Gaming & Linux Compatibility
- [ ] glibc compatibility
- [ ] pthread compatibility
- [ ] futex support
- [ ] epoll support
- [ ] SDL2
- [ ] SDL3
- [ ] OpenGL
- [ ] Vulkan
- [ ] OpenAL
- [ ] Steam Runtime
- [ ] Wine compatibility
- [ ] Proton compatibility
- [ ] Run Doom
- [ ] Run Quake
- [ ] Run Quake III
- [ ] Run Minecraft
- [ ] Run Steam
- [ ] Run CS:GO
- [ ] Run Dota 2
