#pragma once

#include "efi_defs.hpp"

class UEFIApplication {
private:
    static UEFIApplication* s_Instance;
    EFI_HANDLE m_ImageHandle;
    EFI_SYSTEM_TABLE* m_SystemTable;

public:
    UEFIApplication(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE* systemTable);
    ~UEFIApplication();

    static UEFIApplication& Get() { return *s_Instance; }

    EFI_HANDLE GetImageHandle() const { return m_ImageHandle; }
    EFI_SYSTEM_TABLE* GetSystemTable() const { return m_SystemTable; }
    EFI_BOOT_SERVICES* GetBootServices() const { return m_SystemTable->BootServices; }
    EFI_RUNTIME_SERVICES* GetRuntimeServices() const { return m_SystemTable->RuntimeServices; }

    // Helper to search and retrieve protocols
    EFI_STATUS LocateProtocol(const EFI_GUID& guid, void** interface) const;

    // Helper to find ACPI/SMBIOS configuration tables
    void* FindConfigurationTable(const EFI_GUID& guid) const;

    // Stalls thread execution for a given number of microseconds
    void Stall(size_t microseconds) const;
};
