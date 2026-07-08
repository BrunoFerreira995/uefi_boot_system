#pragma once

#include "efi_defs.hpp"
#include "simple_file_system.hpp"

class File {
private:
    char16_t m_Path[256];
    EFI_FILE_PROTOCOL* m_Handle;
    bool m_OwnsHandle;
    static EFI_FILE_PROTOCOL* s_RootVolume;

public:
    File(const char* asciiPath);
    File(const char16_t* utf16Path);
    ~File();

    // Initializes the static root volume interface from the Loaded Image Device Handle
    static bool InitRootVolume(EFI_HANDLE imageHandle);
    static void CloseRootVolume();

    bool Open();
    void Close();
    size_t GetSize();
    size_t Read(void* buffer, size_t size);
    bool SetPosition(uint64_t position);

    bool IsOpen() const { return m_Handle != nullptr; }

    // Directory listing helper
    static void ListDirectory(const char* asciiPath);
};
