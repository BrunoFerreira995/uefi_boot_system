cmake_minimum_required(VERSION 3.20)
project(uefi_boot_system NONE)

include(ExternalProject)

# Orchestrate the bootloader project compilation (using the MinGW toolchain)
ExternalProject_Add(bootloader_proj
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bootloader
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/bootloader
    CMAKE_ARGS
        -DCMAKE_SYSTEM_NAME=Windows
        -DCMAKE_CXX_COMPILER=/opt/homebrew/bin/x86_64-w64-mingw32-g++
        -DCMAKE_C_COMPILER=/opt/homebrew/bin/x86_64-w64-mingw32-gcc
    INSTALL_COMMAND ""
)

# Orchestrate the kernel project compilation (using Clang/LLVM assembler)
ExternalProject_Add(kernel_proj
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/kernel
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/kernel
    CMAKE_ARGS
        -DCMAKE_SYSTEM_NAME=Generic
        -DCMAKE_CXX_COMPILER=clang++
        -DCMAKE_C_COMPILER=clang
        -DCMAKE_ASM_COMPILER=clang
    INSTALL_COMMAND ""
)
