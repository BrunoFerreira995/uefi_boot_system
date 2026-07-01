#include "uefi_application.hpp"

UEFIApplication* UEFIApplication::s_Instance = nullptr;

UEFIApplication::UEFIApplication(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE* systemTable)
    : m_ImageHandle(imageHandle), m_SystemTable(systemTable) {
    s_Instance = this;
}

UEFIApplication::~UEFIApplication() {
    if (s_Instance == this) {
        s_Instance = nullptr;
    }
}

EFI_STATUS UEFIApplication::LocateProtocol(const EFI_GUID& guid, void** interface) const {
    if (!m_SystemTable || !m_SystemTable->BootServices || !m_SystemTable->BootServices->LocateProtocol) {
        return EFI_UNSUPPORTED;
    }
    return m_SystemTable->BootServices->LocateProtocol(&guid, nullptr, interface);
}

void* UEFIApplication::FindConfigurationTable(const EFI_GUID& guid) const {
    if (!m_SystemTable) return nullptr;
    for (size_t i = 0; i < m_SystemTable->NumberOfTableEntries; ++i) {
        if (m_SystemTable->ConfigurationTable[i].VendorGuid == guid) {
            return m_SystemTable->ConfigurationTable[i].VendorTable;
        }
    }
    return nullptr;
}

void UEFIApplication::Stall(size_t microseconds) const {
    if (m_SystemTable && m_SystemTable->BootServices && m_SystemTable->BootServices->Stall) {
        using EFI_STALL = EFI_STATUS (EFIAPI *)(size_t Microseconds);
        ((EFI_STALL)m_SystemTable->BootServices->Stall)(microseconds);
    }
}
