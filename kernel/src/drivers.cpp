#include "drivers.hpp"

#include "kernel.hpp"

namespace {

static constexpr uint16_t kPs2DataPort = 0x60;
static constexpr uint16_t kPs2StatusPort = 0x64;
static constexpr uint16_t kPs2CommandPort = 0x64;
static constexpr uint16_t kPciConfigAddress = 0xCF8;
static constexpr uint16_t kPciConfigData = 0xCFC;
static constexpr uint32_t kPciInvalidVendor = 0xFFFF;

DriverStatus g_Status {};

uint8_t In8(uint16_t port) {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void Out8(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" :: "a"(value), "Nd"(port));
}

uint32_t In32(uint16_t port) {
    uint32_t value;
    asm volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void Out32(uint16_t port, uint32_t value) {
    asm volatile("outl %0, %1" :: "a"(value), "Nd"(port));
}

bool WaitPs2InputClear() {
    for (uint32_t retry = 0; retry < 100000; retry++) {
        if ((In8(kPs2StatusPort) & 0x02) == 0) {
            return true;
        }
    }
    return false;
}

bool WaitPs2OutputReady() {
    for (uint32_t retry = 0; retry < 100000; retry++) {
        if ((In8(kPs2StatusPort) & 0x01) != 0) {
            return true;
        }
    }
    return false;
}

bool Ps2WriteCommand(uint8_t command) {
    if (!WaitPs2InputClear()) {
        return false;
    }
    Out8(kPs2CommandPort, command);
    return true;
}

bool Ps2WriteData(uint8_t data) {
    if (!WaitPs2InputClear()) {
        return false;
    }
    Out8(kPs2DataPort, data);
    return true;
}

bool Ps2Read(uint8_t& value) {
    if (!WaitPs2OutputReady()) {
        return false;
    }
    value = In8(kPs2DataPort);
    return true;
}

void FlushPs2Output() {
    for (uint32_t i = 0; i < 32 && (In8(kPs2StatusPort) & 0x01) != 0; i++) {
        static_cast<void>(In8(kPs2DataPort));
    }
}

bool InitKeyboard() {
    FlushPs2Output();

    if (!Ps2WriteData(0xF4)) {
        return false;
    }

    uint8_t response = 0;
    if (!Ps2Read(response)) {
        return false;
    }

    return response == 0xFA;
}

bool InitMouse() {
    uint8_t response = 0;

    if (!Ps2WriteCommand(0xA8)) {
        return false;
    }

    if (!Ps2WriteCommand(0xD4) || !Ps2WriteData(0xF4)) {
        return false;
    }

    if (!Ps2Read(response)) {
        return false;
    }

    return response == 0xFA;
}

uint32_t PciReadConfig(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    const uint32_t address =
        (1u << 31) |
        (static_cast<uint32_t>(bus) << 16) |
        (static_cast<uint32_t>(device) << 11) |
        (static_cast<uint32_t>(function) << 8) |
        (offset & 0xFC);

    Out32(kPciConfigAddress, address);
    return In32(kPciConfigData);
}

uint16_t PciVendorId(uint8_t bus, uint8_t device, uint8_t function) {
    return static_cast<uint16_t>(PciReadConfig(bus, device, function, 0x00) & 0xFFFF);
}

void ClassifyPciDevice(uint8_t bus, uint8_t device, uint8_t function) {
    const uint32_t class_reg = PciReadConfig(bus, device, function, 0x08);
    const uint8_t class_code = static_cast<uint8_t>((class_reg >> 24) & 0xFF);
    const uint8_t subclass = static_cast<uint8_t>((class_reg >> 16) & 0xFF);

    g_Status.pci_device_count++;

    if (class_code == 0x0C && subclass == 0x03) {
        g_Status.usb_controller_count++;
    } else if (class_code == 0x01 && subclass == 0x08) {
        g_Status.nvme_controller_count++;
    } else if (class_code == 0x02) {
        g_Status.network_controller_count++;
    }
}

void ScanPciFunction(uint8_t bus, uint8_t device, uint8_t function) {
    if (PciVendorId(bus, device, function) == kPciInvalidVendor) {
        return;
    }

    ClassifyPciDevice(bus, device, function);
}

void ScanPciDevice(uint8_t bus, uint8_t device) {
    if (PciVendorId(bus, device, 0) == kPciInvalidVendor) {
        return;
    }

    ScanPciFunction(bus, device, 0);

    const uint32_t header_reg = PciReadConfig(bus, device, 0, 0x0C);
    const uint8_t header_type = static_cast<uint8_t>((header_reg >> 16) & 0xFF);
    if ((header_type & 0x80) == 0) {
        return;
    }

    for (uint8_t function = 1; function < 8; function++) {
        ScanPciFunction(bus, device, function);
    }
}

void ScanPciBus(uint8_t bus) {
    for (uint8_t device = 0; device < 32; device++) {
        ScanPciDevice(bus, device);
    }
}

void InitPci() {
    for (uint16_t bus = 0; bus < 256; bus++) {
        ScanPciBus(static_cast<uint8_t>(bus));
    }

    g_Status.pci_ready = g_Status.pci_device_count > 0;
    g_Status.usb_supported = g_Status.usb_controller_count > 0;
    g_Status.nvme_supported = g_Status.nvme_controller_count > 0;
    g_Status.network_supported = g_Status.network_controller_count > 0;
}

void ResetDriverStatus() {
    g_Status.framebuffer_ready = false;
    g_Status.keyboard_ready = false;
    g_Status.mouse_ready = false;
    g_Status.pci_ready = false;
    g_Status.usb_supported = false;
    g_Status.nvme_supported = false;
    g_Status.network_supported = false;
    g_Status.pci_device_count = 0;
    g_Status.usb_controller_count = 0;
    g_Status.nvme_controller_count = 0;
    g_Status.network_controller_count = 0;
}

} // namespace

bool KernelDriversInit(const BootInfo& boot_info) {
    ResetDriverStatus();
    g_Status.framebuffer_ready =
        boot_info.framebuffer.base_address != 0 &&
        boot_info.framebuffer.width > 0 &&
        boot_info.framebuffer.height > 0 &&
        boot_info.framebuffer.pixels_per_scanline >= boot_info.framebuffer.width;

    g_Status.keyboard_ready = InitKeyboard();
    g_Status.mouse_ready = InitMouse();
    InitPci();

    KernelLog(LogLevel::Info, "Phase 7 drivers initialized");
    return g_Status.framebuffer_ready;
}

const DriverStatus& KernelDriversStatus() {
    return g_Status;
}

void PrintDriverInfo() {
    KernelLog(LogLevel::Info, "Driver status");

    KernelLog(g_Status.framebuffer_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.framebuffer_ready ? "Framebuffer driver ready" : "Framebuffer driver unavailable");
    KernelLog(g_Status.keyboard_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.keyboard_ready ? "PS/2 keyboard ready" : "PS/2 keyboard not found");
    KernelLog(g_Status.mouse_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.mouse_ready ? "PS/2 mouse ready" : "PS/2 mouse not found");

    KernelLog(g_Status.pci_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.pci_ready ? "PCI bus enumerated" : "PCI bus has no visible devices");

    KernelLog(g_Status.usb_supported ? LogLevel::Info : LogLevel::Warn,
        g_Status.usb_supported ? "USB controller support active" : "USB controller not found");
    KernelLog(g_Status.nvme_supported ? LogLevel::Info : LogLevel::Warn,
        g_Status.nvme_supported ? "NVMe controller support active" : "NVMe controller not found");
    KernelLog(g_Status.network_supported ? LogLevel::Info : LogLevel::Warn,
        g_Status.network_supported ? "Network controller support active" : "Network controller not found");
}
