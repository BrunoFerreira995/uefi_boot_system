#include "file.hpp"
#include "loaded_image.hpp"
#include "uefi_application.hpp"
#include "efi_console.hpp"

EFI_FILE_PROTOCOL* File::s_RootVolume = nullptr;

File::File(const char* asciiPath) : m_Handle(nullptr), m_OwnsHandle(true) {
    // Convert ASCII to UTF-16 and replace '/' with '\'
    size_t i = 0;
    // Skip leading slash if any, but UEFI usually takes paths relative to the volume root.
    // If it starts with '/', we can skip it or keep it. Standard UEFI paths relative to root don't have leading backslash,
    // or they do. Let's handle it by stripping the leading slash, or preserving. Usually starting with backslash is fine.
    if (asciiPath[0] == '/') {
        asciiPath++;
    }

    while (asciiPath[i] != '\0' && i < 254) {
        char c = asciiPath[i];
        if (c == '/') {
            c = '\\';
        }
        m_Path[i] = static_cast<char16_t>(c);
        i++;
    }
    m_Path[i] = L'\0';
}

File::File(const char16_t* utf16Path) : m_Handle(nullptr), m_OwnsHandle(true) {
    size_t i = 0;
    if (utf16Path[0] == L'/') {
        utf16Path++;
    }

    while (utf16Path[i] != L'\0' && i < 254) {
        char16_t c = utf16Path[i];
        if (c == L'/') {
            c = L'\\';
        }
        m_Path[i] = c;
        i++;
    }
    m_Path[i] = L'\0';
}

File::~File() {
    Close();
}

bool File::InitRootVolume(EFI_HANDLE imageHandle) {
    if (s_RootVolume) return true;

    auto& app = UEFIApplication::Get();
    EFI_BOOT_SERVICES* bs = app.GetBootServices();

    // Get Loaded Image Protocol
    EFI_LOADED_IMAGE_PROTOCOL* loadedImage = nullptr;
    EFI_STATUS status = bs->HandleProtocol(imageHandle, &EFI_LOADED_IMAGE_PROTOCOL_GUID, reinterpret_cast<void**>(&loadedImage));
    if (status != EFI_SUCCESS || !loadedImage) {
        EFIConsole::PrintFormatted("Error: Failed to locate Loaded Image Protocol (%x)\n", status);
        return false;
    }

    // Get Simple File System Protocol from Loaded Image Device Handle
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* sfs = nullptr;
    status = bs->HandleProtocol(loadedImage->DeviceHandle, &EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID, reinterpret_cast<void**>(&sfs));
    if (status != EFI_SUCCESS || !sfs) {
        EFIConsole::PrintFormatted("Error: Failed to locate Simple File System Protocol (%x)\n", status);
        return false;
    }

    // Open Root Volume
    status = sfs->OpenVolume(sfs, &s_RootVolume);
    if (status != EFI_SUCCESS || !s_RootVolume) {
        EFIConsole::PrintFormatted("Error: Failed to open Root Volume (%x)\n", status);
        return false;
    }

    return true;
}

void File::CloseRootVolume() {
    if (s_RootVolume) {
        s_RootVolume->Close(s_RootVolume);
        s_RootVolume = nullptr;
    }
}

bool File::Open() {
    if (!s_RootVolume) return false;
    if (m_Handle) return true;

    if (m_Path[0] == L'\0') {
        m_Handle = s_RootVolume;
        m_OwnsHandle = false;
        m_Handle->SetPosition(m_Handle, 0);
        return true;
    }

    EFI_STATUS status = s_RootVolume->Open(
        s_RootVolume,
        &m_Handle,
        m_Path,
        EFI_FILE_MODE_READ,
        0
    );

    if (status != EFI_SUCCESS) {
        m_Handle = nullptr;
        return false;
    }

    return true;
}

void File::Close() {
    if (m_Handle) {
        if (m_OwnsHandle) {
            m_Handle->Close(m_Handle);
        }
        m_Handle = nullptr;
        m_OwnsHandle = true;
    }
}

size_t File::GetSize() {
    if (!m_Handle) return 0;

    size_t infoSize = 512;
    alignas(alignof(EFI_FILE_INFO)) char buffer[512];
    EFI_STATUS status = m_Handle->GetInfo(
        m_Handle,
        &EFI_FILE_INFO_GUID,
        &infoSize,
        buffer
    );

    if (status == EFI_SUCCESS) {
        EFI_FILE_INFO* info = reinterpret_cast<EFI_FILE_INFO*>(buffer);
        return info->FileSize;
    }

    return 0;
}

size_t File::Read(void* buffer, size_t size) {
    if (!m_Handle) return 0;

    size_t readSize = size;
    EFI_STATUS status = m_Handle->Read(m_Handle, &readSize, buffer);
    if (status == EFI_SUCCESS) {
        return readSize;
    }

    return 0;
}

bool File::SetPosition(uint64_t position) {
    if (!m_Handle) return false;
    return m_Handle->SetPosition(m_Handle, position) == EFI_SUCCESS;
}

void File::ListDirectory(const char* asciiPath) {
    File dir(asciiPath);
    if (!dir.Open()) {
        EFIConsole::PrintFormatted("Failed to open directory: %s\n", asciiPath);
        return;
    }

    EFIConsole::PrintFormatted("Listing directory: %s\n", asciiPath);

    alignas(alignof(EFI_FILE_INFO)) char buffer[512];
    while (true) {
        size_t readSize = sizeof(buffer);
        EFI_STATUS status = dir.m_Handle->Read(dir.m_Handle, &readSize, buffer);
        if (status != EFI_SUCCESS || readSize == 0) {
            break; // End of directory or error
        }

        EFI_FILE_INFO* info = reinterpret_cast<EFI_FILE_INFO*>(buffer);
        // Direct conversion of UTF-16 to ASCII representation for printing
        char name[128];
        size_t j = 0;
        while (info->FileName[j] != L'\0' && j < 127) {
            name[j] = static_cast<char>(info->FileName[j]);
            j++;
        }
        name[j] = '\0';

        if (info->Attribute & EFI_FILE_DIRECTORY) {
            EFIConsole::PrintFormatted("  [DIR]  %s\n", name);
        } else {
            EFIConsole::PrintFormatted("  [FILE] %s (%llu bytes)\n", name, info->FileSize);
        }
    }
    dir.Close();
}
