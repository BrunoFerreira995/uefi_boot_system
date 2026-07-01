cmake_minimum_required(VERSION 3.20)
project(bootloader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# FREESTANDING UEFI COMPILE FLAGS
add_compile_options(
    -nostdlib
    -ffreestanding
    -fno-exceptions
    -fno-rtti
    -fno-threadsafe-statics
    -Wall
    -Wextra
)

# Custom header include directories
include_directories(include)

# Executable target
add_executable(BOOTX64.EFI
    src/main.cpp
    src/uefi_application.cpp
    src/efi_console.cpp
    src/file.cpp
    src/elf_loader.cpp
    src/memory_map.cpp
    src/framebuffer.cpp
)

# Link options to build PE32+ DLL (subsystem 10 is UEFI Application)
target_link_options(BOOTX64.EFI PRIVATE
    -shared
    -nostdlib
    -Wl,-dll
    -Wl,--subsystem,10
    -e efi_main
)
