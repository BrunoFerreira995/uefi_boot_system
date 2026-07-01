# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/bucha/.gemini/antigravity/scratch/uefi_boot_system/bootloader")
  file(MAKE_DIRECTORY "/Users/bucha/.gemini/antigravity/scratch/uefi_boot_system/bootloader")
endif()
file(MAKE_DIRECTORY
  "/Users/bucha/.gemini/antigravity/scratch/uefi_boot_system/build/bootloader"
  "/Users/bucha/.gemini/antigravity/scratch/uefi_boot_system/build/bootloader_proj-prefix"
  "/Users/bucha/.gemini/antigravity/scratch/uefi_boot_system/build/bootloader_proj-prefix/tmp"
  "/Users/bucha/.gemini/antigravity/scratch/uefi_boot_system/build/bootloader_proj-prefix/src/bootloader_proj-stamp"
  "/Users/bucha/.gemini/antigravity/scratch/uefi_boot_system/build/bootloader_proj-prefix/src"
  "/Users/bucha/.gemini/antigravity/scratch/uefi_boot_system/build/bootloader_proj-prefix/src/bootloader_proj-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/bucha/.gemini/antigravity/scratch/uefi_boot_system/build/bootloader_proj-prefix/src/bootloader_proj-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/bucha/.gemini/antigravity/scratch/uefi_boot_system/build/bootloader_proj-prefix/src/bootloader_proj-stamp${cfgdir}") # cfgdir has leading slash
endif()
