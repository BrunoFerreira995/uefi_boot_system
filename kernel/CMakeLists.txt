cmake_minimum_required(VERSION 3.20)
project(kernel LANGUAGES CXX ASM)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Object-only target to compile source files to ELF object format
add_library(kernel_objs OBJECT
    src/entry.S
    src/interrupt_stubs.S
    src/context_switch.S
    src/cpu.cpp
    src/drivers.cpp
    src/filesystem.cpp
    src/gui.cpp
    src/network.cpp
    src/performance.cpp
    src/scheduler.cpp
    src/security.cpp
    src/userspace.cpp
    src/kernel.cpp
)

# Apply modern compile flags to the target
target_compile_options(kernel_objs PRIVATE
    -target x86_64-unknown-elf
    -nostdlib
    -ffreestanding
    -fno-exceptions
    -fno-rtti
    -mno-red-zone
    -Wall
    -Wextra
)

target_include_directories(kernel_objs PRIVATE include)

# Link the compiled object files using x86_64-elf-ld directly
add_custom_target(kernel.elf ALL
    COMMAND /opt/homebrew/bin/x86_64-elf-ld -Ttext 0x1000000 -entry _start -o ${CMAKE_CURRENT_BINARY_DIR}/kernel.elf ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/kernel_objs.dir/src/entry.S.obj ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/kernel_objs.dir/src/interrupt_stubs.S.obj ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/kernel_objs.dir/src/context_switch.S.obj ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/kernel_objs.dir/src/cpu.cpp.obj ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/kernel_objs.dir/src/drivers.cpp.obj ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/kernel_objs.dir/src/filesystem.cpp.obj ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/kernel_objs.dir/src/gui.cpp.obj ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/kernel_objs.dir/src/network.cpp.obj ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/kernel_objs.dir/src/performance.cpp.obj ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/kernel_objs.dir/src/scheduler.cpp.obj ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/kernel_objs.dir/src/security.cpp.obj ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/kernel_objs.dir/src/userspace.cpp.obj ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/kernel_objs.dir/src/kernel.cpp.obj
    DEPENDS kernel_objs
    COMMENT "Linking kernel.elf using x86_64-elf-ld..."
)
