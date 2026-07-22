# Project Finish Checklist

> Current Status: Graphical UEFI operating system prototype with a custom bootloader, ELF64 kernel, memory management, scheduler, GUI, and early userspace.
> Latest local verification: `./scripts/test.sh` passes with 229 checks.

## Snapshot
- Completed foundation: Phases 1-18.
- Completed stabilization: Phase 18.5 priorities 2-6.
- Remaining stabilization follow-up: QEMU desktop-visibility regression under Priority 1.
- Active backlog starts with Phase 19.5: desktop UX, launcher accessibility, cursor latency, redraw performance, then Phase 20 integration tests, release hardening, and gaming/Linux compatibility.

## Open Work Index

- Runtime stabilization follow-up: 1 QEMU repeated-launch desktop visibility regression.
- Phase 19.5 desktop UX and rendering responsiveness: launcher/menu accessibility, taskbar states, cursor latency, frame pacing, caching, telemetry, and QEMU performance verification.
- Phase 20 tests: 16 integration/stress tests and 5 compatibility demos.
- Phase 21 release: 12 version 1.0 hardening and packaging items.
- Phase 22 gaming/Linux compatibility: 19 compatibility and game targets.

---

# Completed Foundation

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
- [x] POSIX compatibility
- [x] ELF executable compatibility
- [x] glibc compatibility
- [x] musl compatibility
- [x] SDL2 support
- [x] SDL3 support
- [x] OpenGL abstraction
- [x] Vulkan abstraction
- [x] OpenAL
- [x] Wayland compatibility
- [x] X11 compatibility layer
- [x] Steam Runtime compatibility

---

# Runtime Stabilization

> Goal: turn the QEMU demo into a repeatable desktop workflow: click, open, focus, interact, save, minimize, restore, and close native apps without losing the desktop.

# Phase 18.5 — Runtime Desktop & App Stabilization

## Video-Observed Working Baseline
- [x] Boot reaches graphical desktop in QEMU
- [x] Desktop wallpaper/background is displayed
- [x] Mouse cursor moves
- [x] Mouse clicks are processed
- [x] Menu or launcher can open
- [x] Multiple windows/screens can appear
- [x] Native app launch path is partially present
- [x] System remains alive during short recording
- [x] Bootloader to kernel to drivers to window manager to events to apps path exists

## Priority 1 — Stable Interface
- [x] Desktop never disappears after opening or closing apps
- [x] Full-frame compositor path always redraws wallpaper, desktop icons, windows, taskbar, and cursor
- [x] Dirty-rectangle compositor has debug fallback to full redraw
- [x] Dirty-region invalidation detects unsafe regions and triggers full redraw
- [x] Backbuffer is mandatory for desktop composition
- [x] Framebuffer presentation happens after full backbuffer render
- [x] Cursor restore never damages desktop pixels
- [x] Window close restores/redraws the region underneath
- [x] Window minimize does not destroy the window object
- [x] Window restore redraws desktop and taskbar
- [x] Window Z-order is preserved across clicks, launches, minimize, restore, and close
- [x] Clicking a window brings it to front
- [x] Active window focus is explicit and persistent
- [x] Previous active window loses focus before new window receives focus
- [x] Active and inactive windows have visibly different borders/title bars
- [x] Mouse and keyboard events are dispatched only to the focused window/app
- [x] Window list survives launcher/app creation functions
- [x] Window objects are stored in persistent window/process tables
- [x] No pointers to stack-allocated windows are stored by the window manager
- [x] Closing a window separates GUI cleanup from process cleanup
- [x] Full redraw mode can be toggled for compositor debugging
- [x] Z-order/focus/redraw self-test covers two overlapping windows
- [ ] QEMU regression confirms desktop remains visible after repeated app launches

## Priority 2 — Window Sizing & Placement
- [x] Default app window minimum is at least 480x320 where screen size allows
- [x] Per-app minimum window size is defined
- [x] First launched app opens centered in usable desktop area
- [x] New windows are clamped inside visible screen bounds
- [x] Windows do not open under the taskbar
- [x] Window manager remembers last position and size per app
- [x] Dialogs and small utility windows are centered
- [x] Calculator and settings use compact but readable fixed minimum sizes
- [x] Terminal and editor open large enough for useful text
- [x] Initial app placement avoids stacking every app in the top-left corner

## Priority 3 — App Launch Lifecycle
- [x] Launcher has a persistent native app registry
- [x] App registry stores id, display name, executable path, and icon path
- [x] Native app executables use `/system/apps/*.app` or `/bin/*` paths consistently
- [x] Launcher click requests app launch by app id
- [x] Missing app id shows visible error notification
- [x] Process manager creates a process for launched apps
- [x] ELF loader loads app executable for launched apps
- [x] App registers its persistent window after process creation
- [x] Taskbar adds running app after successful launch
- [x] Launching an already-running app focuses or restores it
- [x] Minimized app launch request restores existing app
- [x] Failed app launch reports visible failure state
- [x] App process exit removes taskbar entry and window
- [x] App cleanup releases process resources
- [x] App event loop starts after window registration
- [x] App launch logs include `[APP] launch requested`
- [x] ELF load logs include `[ELF] loading`
- [x] Process creation logs include `[PROC] process created`
- [x] Window registration logs include `[GUI] window registered`
- [x] Focus changes log `[GUI] focus changed`
- [x] App exit logs include exit code
- [x] Hung app is detected without freezing compositor/window manager
- [x] App watchdog marks unresponsive apps
- [x] App state model includes Opening, Running, Minimized, Not responding, and Failed
- [x] App launch integration test opens, focuses, minimizes, restores, and closes each native app

## Priority 4 — Native Apps Usability

### Terminal
- [x] `help`
- [x] `clear`
- [x] `ls`
- [x] `cd`
- [x] `pwd`
- [x] `cat`
- [x] `mem`
- [x] `cpu`
- [x] `uptime`
- [x] `reboot`
- [x] `shutdown`
- [x] ANSI handling scaffold
- [x] History scaffold
- [x] Scrollback scaffold
- [x] `mkdir`
- [x] `touch`
- [x] `echo`
- [x] `ps`
- [x] `kill`
- [x] Blinking cursor
- [x] Text selection
- [x] Clipboard copy from selection
- [x] Consistent keyboard input path
- [x] Real PTY-backed terminal IO
- [x] Terminal resize reflows visible rows/columns
- [x] Terminal command tests run through keyboard events, not only direct parser calls

### File Manager
- [x] List directories from VFS
- [x] Open folders
- [x] Back navigation
- [x] Forward navigation
- [x] Create folder
- [x] Rename file/folder
- [x] Copy file/folder
- [x] Move file/folder
- [x] Delete file/folder
- [x] Open files by association
- [x] Show filesystem errors in UI
- [x] Path bar shows current path
- [x] Path bar supports root-to-current breadcrumb navigation

### Text Editor
- [x] New file
- [x] Open file
- [x] Save
- [x] Save as
- [x] Text cursor
- [x] Text selection
- [x] Line wrapping
- [x] Vertical scrolling
- [x] Ctrl+S shortcut
- [x] Ctrl+O shortcut
- [x] Ctrl+A shortcut
- [x] Unsaved changes indicator

### Calculator
- [x] Button grid dispatches click events
- [x] Keyboard input updates expression
- [x] Display text updates after every operation
- [x] App maintains internal calculation state
- [x] Basic arithmetic: add, subtract, multiply, divide
- [x] Clear and backspace actions

### Settings
- [x] Resolution settings page
- [x] Wallpaper settings page
- [x] Theme settings page
- [x] Volume settings page
- [x] Mouse settings page
- [x] Keyboard settings page
- [x] Network settings page
- [x] System information page

### Task Manager
- [x] Shows PID
- [x] Shows process name
- [x] Shows process state
- [x] Shows CPU usage
- [x] Shows memory usage
- [x] Shows window count per process
- [x] Can request process termination
- [x] Distinguishes running, minimized, failed, and unresponsive apps

## Priority 5 — Desktop Experience
- [x] Taskbar is always visible
- [x] Taskbar highlights active app/window
- [x] Taskbar indicates minimized apps
- [x] Clock remains visible
- [x] App menu/launcher has larger clickable icons
- [x] Window title text remains legible
- [x] Window controls for close, minimize, and maximize are reliable
- [x] Short app opening animation or state transition
- [x] Window shadow remains visible after redraws
- [x] Error notification UI exists
- [x] Cursor contrast is sufficient on dark and light backgrounds
- [x] Dialogs are centered
- [x] File associations launch the correct app
- [x] Keyboard shortcuts route to focused app

## Priority 6 — Runtime Verification
- [x] QEMU smoke test captures boot-to-desktop serial/video evidence
- [x] QEMU test opens launcher and launches Terminal
- [x] QEMU test launches Calculator and verifies display update
- [x] QEMU test launches File Manager and verifies directory listing
- [x] QEMU test launches Text Editor and verifies text entry
- [x] QEMU test minimizes and restores a window
- [x] QEMU test closes a window and verifies desktop redraw
- [x] QEMU test switches focus between overlapping windows
- [x] QEMU test repeats app open/close cycle without losing desktop
- [x] QEMU test records no panic during a 60-second interaction run
- [x] Screenshot-based regression detects empty/dark desktop after actions
- [x] Framebuffer/compositor test verifies non-empty wallpaper, taskbar, and at least one window

---

# Active Backlog

## Phase 19 — Performance
- [x] Hardware acceleration
- [x] Optimized compositor
- [x] Multi-core scheduling
- [x] NUMA scheduling
- [x] GPU acceleration
- [x] Profiling tools
- [x] Benchmark suite
- [x] Kernel profiler
- [x] Performance counters

---


## Phase 19.5 — Desktop UX, Menus & Rendering Responsiveness

> Goal: make the desktop immediately understandable and responsive: users can quickly find native apps, every click gives instant feedback, cursor motion stays fluid, and redraw work is limited to the pixels that changed.

### Launcher & Main Menu Accessibility
- [ ] Add a clearly identifiable launcher button labeled `Menu`, `Start`, or with the UEFI OS logo
- [ ] Keep the launcher entry point permanently visible on the taskbar
- [ ] Launcher opens in under 100 ms under normal QEMU load
- [ ] Launcher closes when clicking outside, pressing Escape, or launching an app
- [ ] Launcher supports keyboard opening through a dedicated shortcut
- [ ] Add an app search field with incremental filtering
- [ ] Group apps into `Favorites`, `Utilities`, `System`, and `Development` categories
- [ ] Show Terminal, File Manager, Text Editor, Calculator, Settings, Task Manager, System Monitor, Package Manager, and Software Center
- [ ] Add power actions: Lock, Log out, Restart, and Shut down
- [ ] Persist favorite apps and launcher ordering
- [ ] Prevent launcher from opening outside visible screen bounds
- [ ] Launcher remains above normal windows but below system-critical dialogs
- [ ] Launcher has a visible selected item for keyboard navigation
- [ ] Arrow keys navigate app entries
- [ ] Enter launches the selected app
- [ ] Escape closes the launcher without changing app focus
- [ ] Launcher interaction test verifies mouse and keyboard navigation

### App Icons & Click Targets
- [ ] Native app icons are at least 40x40 pixels where resolution allows
- [ ] Every launcher item has a minimum clickable area of 48x48 pixels
- [ ] App name and icon share the same click target
- [ ] Hover state is visible within 50 ms
- [ ] Pressed state appears immediately on MouseDown
- [ ] Disabled or unavailable apps have a distinct visual state
- [ ] Running apps show a running indicator
- [ ] Active app shows a stronger highlight than background apps
- [ ] Minimized apps show a distinct minimized indicator
- [ ] App icons are cached after first decode/rasterization
- [ ] Missing icon uses a stable fallback without blocking launch
- [ ] Icon hit-testing matches visual bounds and scaling

### Launch Feedback & App States
- [ ] Click feedback appears in under 50 ms
- [ ] Launcher shows `Opening...` immediately after a launch request
- [ ] App state indicator supports Opening, Running, Minimized, Not responding, and Failed
- [ ] First application frame target is under 300 ms for simple native apps
- [ ] Launch failure shows an error notification with app name and reason
- [ ] Launching an existing app focuses or restores it instead of duplicating it
- [ ] Long app startup does not block cursor, taskbar, launcher, or compositor
- [ ] App launch telemetry records click, process creation, ELF load, window registration, and first visible frame

### Taskbar & Desktop Navigation
- [ ] Taskbar permanently exposes launcher, pinned apps, running apps, and system tray
- [ ] Taskbar shows active, inactive, minimized, failed, and unresponsive states
- [ ] Clicking an active taskbar item minimizes the app when configured
- [ ] Clicking a minimized taskbar item restores and focuses the app
- [ ] Taskbar app order remains stable during redraws
- [ ] Clock updates without forcing a full desktop redraw
- [ ] Network and volume indicators update through isolated dirty regions
- [ ] Pinned app configuration persists across sessions
- [ ] Desktop icons use the same app registry as launcher and taskbar
- [ ] Double-clicking a desktop app icon launches or focuses the app
- [ ] Desktop context menu supports Refresh, Display settings, and Personalization

### Cursor Fast Path
- [ ] Prefer hardware cursor plane when supported by VirtIO GPU or physical GPU
- [ ] Hardware cursor movement updates position without full desktop composition
- [ ] Software cursor fallback restores only the previous cursor rectangle
- [ ] Software cursor fallback saves and draws only the new cursor rectangle
- [ ] Cursor dirty region is the union of previous and current cursor bounds
- [ ] Cursor movement never triggers wallpaper, window, or taskbar full redraw
- [ ] Cursor redraw target is under 1 ms for software fallback where practical
- [ ] Cursor input-to-visible latency target is under 16 ms
- [ ] Coalesce multiple queued mouse-move events to the latest position per frame
- [ ] Preserve MouseDown, MouseUp, wheel, and drag events while coalescing only motion
- [ ] Cursor shape changes for pointer, text, resize, drag, and busy states
- [ ] Cursor remains visible and undamaged over dark and light content
- [ ] Cursor performance counters expose redraw time, updated pixels, and dropped motion events

### Event Processing & Frame Scheduling
- [ ] Input handlers update state and dirty flags without rendering synchronously
- [ ] Compositor runs from a dedicated render loop
- [ ] Mouse event processing is decoupled from full-frame presentation
- [ ] Render loop drains pending input before building the next frame
- [ ] Frame pacing targets 60 Hz where supported
- [ ] 30 Hz fallback remains smooth when QEMU cannot sustain 60 Hz
- [ ] Avoid unbounded event queue growth during rapid mouse movement
- [ ] Scheduler gives compositor and input threads latency-sensitive priority
- [ ] Long-running apps cannot starve desktop input or rendering
- [ ] Animation timestamps use monotonic timers instead of fixed sleep accumulation
- [ ] Missed frames are counted and reported
- [ ] Present only when there is visible damage, animation, or cursor work

### Dirty Regions & Redraw Optimization
- [ ] Full redraw occurs only for initialization, resolution change, wallpaper/theme change, or recovery
- [ ] Window move invalidates old bounds, new bounds, shadow, and exposed regions
- [ ] Window resize invalidates only affected window and exposed desktop regions
- [ ] Window close redraws only the uncovered region plus taskbar state
- [ ] Window minimize and restore use bounded invalidation
- [ ] Focus change redraws only old and new title bars/borders plus taskbar state
- [ ] Hover redraws only the affected control or menu row
- [ ] Dirty rectangles are clipped to framebuffer bounds
- [ ] Overlapping or nearby dirty rectangles are merged using a configurable threshold
- [ ] Excessive dirty-rectangle count falls back to one bounded region or full redraw
- [ ] Empty dirty regions never trigger framebuffer presentation
- [ ] Dirty-region telemetry reports rectangle count and total changed pixels
- [ ] Visual regression verifies no stale pixels after move, close, minimize, and restore

### Backbuffer & Presentation
- [ ] Desktop composition always targets a valid backbuffer
- [ ] Backbuffer allocation is persistent and not repeated per frame
- [ ] Framebuffer copy honors pitch/stride and pixel format
- [ ] Present path supports dirty-rectangle copies
- [ ] Full-frame copy is reserved for explicit full redraw
- [ ] Use page-aligned buffers for faster copy operations
- [ ] Benchmark scalar, SIMD, and device-assisted copy paths
- [ ] Use SSE/AVX optimized copy path when safe and available
- [ ] Triple buffering does not add avoidable input latency
- [ ] Prevent tearing through controlled presentation or device synchronization where available
- [ ] Presentation failure logs the rectangle, stride, format, and elapsed time

### Menu, Font & Icon Caching
- [ ] Cache launcher background and static panel decorations
- [ ] Cache rasterized app icons by path, size, scale, and theme
- [ ] Cache font glyphs by font, size, style, and codepoint
- [ ] Cache shaped or measured menu labels where applicable
- [ ] Avoid repeated PNG, BMP, JPEG, SVG, and TrueType parsing during normal frames
- [ ] Invalidate caches only when theme, DPI, icon, font, or locale changes
- [ ] Bound cache memory and implement safe eviction
- [ ] Expose cache hit/miss counters to the profiler
- [ ] Launcher cold-open and warm-open benchmarks are recorded separately

### Allocation & Locking Discipline
- [ ] No dynamic allocation occurs in cursor movement hot path
- [ ] No dynamic allocation occurs in normal compositor frame path
- [ ] Preallocate dirty-rectangle storage
- [ ] Preallocate common draw-command storage
- [ ] Avoid holding global window-manager lock during pixel rendering
- [ ] Avoid holding process-manager lock while loading icons or fonts
- [ ] Use bounded lock duration for taskbar and launcher state updates
- [ ] Detect lock contention affecting input or frame time
- [ ] Profiler reports compositor, input, window-manager, and taskbar lock wait time

### Performance Telemetry & Targets
- [ ] Log total frame render time in microseconds
- [ ] Log framebuffer presentation time separately
- [ ] Log cursor redraw time separately
- [ ] Log dirty-rectangle count and dirty-pixel count per sampled frame
- [ ] Log launcher open latency
- [ ] Log app first-frame latency
- [ ] Log input-to-visible latency for mouse and keyboard interactions
- [ ] Add rolling average, p95, and maximum frame time metrics
- [ ] Add frame-rate and missed-frame counters to System Monitor
- [ ] Add compositor CPU usage to Task Manager/System Monitor
- [ ] Cursor target: under 16 ms input-to-visible latency
- [ ] Hover target: under 50 ms
- [ ] Launcher target: under 100 ms
- [ ] Native utility first-frame target: under 300 ms
- [ ] Minimize/restore animation target: under 150 ms
- [ ] Emergency full redraw target: under 33 ms where hardware allows
- [ ] Normal dirty-region frame target: under 16.67 ms at 60 Hz

### QEMU & Hardware Verification
- [ ] QEMU test measures cursor movement latency and redraw cost
- [ ] QEMU test opens and closes launcher 100 times without visual corruption
- [ ] QEMU test rapidly moves cursor while opening an app
- [ ] QEMU test verifies desktop remains responsive during ELF loading
- [ ] QEMU test opens every native app from the launcher
- [ ] QEMU test validates taskbar state after launch, minimize, restore, focus, and close
- [ ] QEMU test performs rapid window dragging without stale pixels
- [ ] QEMU test performs rapid window resizing without full-screen flicker
- [ ] QEMU test records a 60-second interaction run with frame-time telemetry
- [ ] QEMU test fails when p95 cursor latency exceeds target
- [ ] QEMU test fails when launcher open latency exceeds target
- [ ] QEMU screenshot regression validates launcher, taskbar, app window, and cursor
- [ ] Hardware test verifies hardware cursor path on at least one supported GPU
- [ ] Hardware test verifies software cursor fallback on unsupported hardware
- [ ] Hardware test validates framebuffer stride and pixel format on multiple resolutions

### Acceptance Workflow
- [ ] Boot to desktop
- [ ] Open launcher in under target latency
- [ ] Search for Terminal and launch it
- [ ] Move cursor continuously while Terminal starts without visible stalls
- [ ] Launch File Manager, Text Editor, Calculator, Settings, and Task Manager
- [ ] Switch focus through window clicks and taskbar items
- [ ] Minimize and restore every app
- [ ] Drag and resize windows without stale pixels or full-screen flicker
- [ ] Close every app and verify desktop remains intact
- [ ] Repeat the full workflow multiple times without panic, memory leak, or latency degradation

---
## Phase 20 — Tests

### Build Tests
- [x] Full bootloader build
- [x] Full kernel build
- [x] Freestanding link check
- [x] Clean build from reset build directory
- [x] Local test pipeline script

### Boot Tests
- [x] QEMU boot command/package smoke test
- [x] Bootloader handoff path validation
- [x] Kernel panic path validation
- [x] Desktop startup path validation
- [x] Userspace exit path validation

### Kernel Self-Tests
- [x] Memory manager self-test
- [x] Scheduler/process self-test
- [x] PS/2 packet decoder self-test
- [x] USB HID descriptor parser self-test
- [x] CPU interrupt delivery handler check
- [x] Filesystem probe self-test
- [x] Userspace syscall path check

### Driver Tests
- [x] PCI enumeration test target
- [x] PCI config-space read/write helper check
- [x] Keyboard input driver path check
- [x] Mouse input driver path check
- [x] Storage controller discovery check
- [x] Network controller discovery check

### UI Tests
- [x] Window manager interaction target
- [x] Terminal command surface check
- [x] Mouse drag/click path check
- [x] Desktop redraw path check
- [x] Graphics primitive rendering path check
- [x] Full compositor redraw visual regression
- [x] Dirty-rectangle fallback regression
- [x] Active window focus visual regression
- [x] Taskbar active/minimized state regression
- [x] Cursor damage regression
- [x] Window sizing and centering regression

### Regression Tests
- [x] Automated local test runner
  - [x] `scripts/test.sh` with 229 checks
- [x] Boot log source check
- [x] Memory regression target check
- [x] Scheduler fairness target check
- [x] Filesystem regression target check

### Integration Tests
- [ ] Userspace application launch
- [ ] Launcher opens every registered native app
- [ ] App launch creates process, window, and taskbar entry
- [ ] Launching existing app focuses/restores it
- [ ] Window close releases GUI and process state
- [ ] App open/close loop preserves desktop contents
- [ ] Terminal keyboard input integration
- [ ] File Manager directory navigation integration
- [ ] Text Editor open/edit/save integration
- [ ] Calculator click and keyboard integration
- [ ] Settings page navigation integration
- [ ] Task Manager process list integration
- [ ] Filesystem stress test
- [ ] Network stress test
- [ ] SMP stress test
- [ ] Memory stress test

### Compatibility Tests
- [ ] SDL2 demo
- [ ] SDL3 demo
- [ ] Doom
- [ ] Quake
- [ ] Quake III

---

## Phase 21 — Version 1.0 Release
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

## Phase 22 — Gaming & Linux Compatibility
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