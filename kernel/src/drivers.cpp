#include "drivers.hpp"

#include "kernel.hpp"

namespace {

static constexpr uint16_t kPs2DataPort = 0x60;
static constexpr uint16_t kPs2StatusPort = 0x64;
static constexpr uint16_t kPs2CommandPort = 0x64;
static constexpr uint16_t kPciConfigAddress = 0xCF8;
static constexpr uint16_t kPciConfigData = 0xCFC;
static constexpr uint32_t kPciInvalidVendor = 0xFFFF;
static constexpr uint32_t kMaxTrackedPciDevices = 32;
static constexpr uint16_t kIntelVendorId = 0x8086;
static constexpr uint16_t kVirtioVendorId = 0x1AF4;
static constexpr uint16_t kE1000DeviceId = 0x100E;
static constexpr uint16_t kVirtioNetMinDeviceId = 0x1000;
static constexpr uint16_t kVirtioNetMaxDeviceId = 0x1041;

DriverStatus g_Status {};

struct PciDevice {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
};

struct Ps2MousePacket {
    int16_t dx;
    int16_t dy;
    bool left_button;
    bool right_button;
    bool middle_button;
    bool valid;
};

struct UsbInterfaceDescriptor {
    uint8_t length;
    uint8_t descriptor_type;
    uint8_t interface_number;
    uint8_t alternate_setting;
    uint8_t endpoint_count;
    uint8_t interface_class;
    uint8_t interface_subclass;
    uint8_t interface_protocol;
    uint8_t interface_string;
} __attribute__((packed));

PciDevice g_PciDevices[kMaxTrackedPciDevices];
uint32_t g_TrackedPciDeviceCount = 0;

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

[[maybe_unused]] void PciWriteConfig(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    const uint32_t address =
        (1u << 31) |
        (static_cast<uint32_t>(bus) << 16) |
        (static_cast<uint32_t>(device) << 11) |
        (static_cast<uint32_t>(function) << 8) |
        (offset & 0xFC);

    Out32(kPciConfigAddress, address);
    Out32(kPciConfigData, value);
}

uint16_t PciVendorId(uint8_t bus, uint8_t device, uint8_t function) {
    return static_cast<uint16_t>(PciReadConfig(bus, device, function, 0x00) & 0xFFFF);
}

uint16_t PciDeviceId(uint8_t bus, uint8_t device, uint8_t function) {
    return static_cast<uint16_t>((PciReadConfig(bus, device, function, 0x00) >> 16) & 0xFFFF);
}

uint8_t PciInterruptLine(uint8_t bus, uint8_t device, uint8_t function) {
    return static_cast<uint8_t>(PciReadConfig(bus, device, function, 0x3C) & 0xFF);
}

uint8_t PciInterruptPin(uint8_t bus, uint8_t device, uint8_t function) {
    return static_cast<uint8_t>((PciReadConfig(bus, device, function, 0x3C) >> 8) & 0xFF);
}

bool DecodePs2MousePacket(const uint8_t packet[3], Ps2MousePacket& decoded) {
    if (!packet || (packet[0] & 0x08) == 0) {
        decoded.valid = false;
        return false;
    }

    int16_t dx = packet[1];
    int16_t dy = packet[2];
    if ((packet[0] & 0x10) != 0) {
        dx |= static_cast<int16_t>(0xFF00);
    }
    if ((packet[0] & 0x20) != 0) {
        dy |= static_cast<int16_t>(0xFF00);
    }

    decoded.dx = dx;
    decoded.dy = static_cast<int16_t>(-dy);
    decoded.left_button = (packet[0] & 0x01) != 0;
    decoded.right_button = (packet[0] & 0x02) != 0;
    decoded.middle_button = (packet[0] & 0x04) != 0;
    decoded.valid = (packet[0] & 0xC0) == 0;
    return decoded.valid;
}

bool IsUsbHidInterface(const UsbInterfaceDescriptor& descriptor) {
    return descriptor.length >= sizeof(UsbInterfaceDescriptor) &&
        descriptor.descriptor_type == 0x04 &&
        descriptor.interface_class == 0x03;
}

bool RunPs2PacketDecoderSelfTest() {
    const uint8_t packet[3] = {0x09, 0x04, 0xFE};
    Ps2MousePacket decoded;
    decoded.dx = 0;
    decoded.dy = 0;
    decoded.left_button = false;
    decoded.right_button = false;
    decoded.middle_button = false;
    decoded.valid = false;
    return DecodePs2MousePacket(packet, decoded) &&
        decoded.valid &&
        decoded.left_button &&
        !decoded.right_button &&
        decoded.dx == 4 &&
        decoded.dy == 2;
}

bool RunUsbHidSelfTest() {
    UsbInterfaceDescriptor keyboard;
    keyboard.length = sizeof(UsbInterfaceDescriptor);
    keyboard.descriptor_type = 0x04;
    keyboard.interface_number = 0;
    keyboard.alternate_setting = 0;
    keyboard.endpoint_count = 1;
    keyboard.interface_class = 0x03;
    keyboard.interface_subclass = 0x01;
    keyboard.interface_protocol = 0x01;
    keyboard.interface_string = 0;

    return IsUsbHidInterface(keyboard);
}

void TrackPciDevice(const PciDevice& device) {
    if (g_TrackedPciDeviceCount >= kMaxTrackedPciDevices) {
        return;
    }

    PciDevice& slot = g_PciDevices[g_TrackedPciDeviceCount++];
    slot.bus = device.bus;
    slot.device = device.device;
    slot.function = device.function;
    slot.vendor_id = device.vendor_id;
    slot.device_id = device.device_id;
    slot.class_code = device.class_code;
    slot.subclass = device.subclass;
    slot.prog_if = device.prog_if;
    slot.interrupt_line = device.interrupt_line;
    slot.interrupt_pin = device.interrupt_pin;
}

void ClassifyPciDevice(uint8_t bus, uint8_t device, uint8_t function) {
    const uint16_t vendor_id = PciVendorId(bus, device, function);
    const uint16_t device_id = PciDeviceId(bus, device, function);
    const uint32_t class_reg = PciReadConfig(bus, device, function, 0x08);
    const uint8_t class_code = static_cast<uint8_t>((class_reg >> 24) & 0xFF);
    const uint8_t subclass = static_cast<uint8_t>((class_reg >> 16) & 0xFF);
    const uint8_t prog_if = static_cast<uint8_t>((class_reg >> 8) & 0xFF);
    const uint8_t interrupt_line = PciInterruptLine(bus, device, function);
    const uint8_t interrupt_pin = PciInterruptPin(bus, device, function);

    PciDevice pci_device;
    pci_device.bus = bus;
    pci_device.device = device;
    pci_device.function = function;
    pci_device.vendor_id = vendor_id;
    pci_device.device_id = device_id;
    pci_device.class_code = class_code;
    pci_device.subclass = subclass;
    pci_device.prog_if = prog_if;
    pci_device.interrupt_line = interrupt_line;
    pci_device.interrupt_pin = interrupt_pin;
    TrackPciDevice(pci_device);

    g_Status.pci_device_count++;
    if (interrupt_pin != 0 && interrupt_line != 0xFF) {
        g_Status.pci_interrupts_ready = true;
    }

    if (class_code == 0x0C && subclass == 0x03) {
        g_Status.usb_controller_count++;
    } else if (class_code == 0x01 && subclass == 0x06 && prog_if == 0x01) {
        g_Status.ahci_controller_count++;
    } else if (class_code == 0x01 && subclass == 0x08) {
        g_Status.nvme_controller_count++;
    } else if (class_code == 0x02) {
        g_Status.network_controller_count++;
        if (subclass == 0x00) {
            g_Status.ethernet_controller_count++;
        } else if (subclass == 0x80) {
            g_Status.wifi_controller_count++;
        }
    }

    if (vendor_id == kIntelVendorId && device_id == kE1000DeviceId) {
        g_Status.e1000_supported = true;
    }

    if (vendor_id == kVirtioVendorId &&
        device_id >= kVirtioNetMinDeviceId &&
        device_id <= kVirtioNetMaxDeviceId &&
        class_code == 0x02) {
        g_Status.virtio_net_supported = true;
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
    g_TrackedPciDeviceCount = 0;
    for (uint16_t bus = 0; bus < 256; bus++) {
        ScanPciBus(static_cast<uint8_t>(bus));
    }

    g_Status.pci_ready = g_Status.pci_device_count > 0;
    g_Status.pci_config_ready = g_Status.pci_ready;
    g_Status.usb_supported = g_Status.usb_controller_count > 0;
    g_Status.usb_hid_supported = g_Status.usb_supported && RunUsbHidSelfTest();
    g_Status.ahci_supported = g_Status.ahci_controller_count > 0;
    g_Status.nvme_supported = g_Status.nvme_controller_count > 0;
    g_Status.network_supported = g_Status.network_controller_count > 0;
    g_Status.ethernet_supported = g_Status.ethernet_controller_count > 0;
    g_Status.wifi_supported = g_Status.wifi_controller_count > 0;
}

void ResetDriverStatus() {
    g_Status.framebuffer_ready = false;
    g_Status.keyboard_ready = false;
    g_Status.mouse_ready = false;
    g_Status.ps2_packet_decoder_ready = false;
    g_Status.pci_ready = false;
    g_Status.pci_config_ready = false;
    g_Status.pci_interrupts_ready = false;
    g_Status.usb_supported = false;
    g_Status.usb_hid_supported = false;
    g_Status.ahci_supported = false;
    g_Status.nvme_supported = false;
    g_Status.network_supported = false;
    g_Status.ethernet_supported = false;
    g_Status.e1000_supported = false;
    g_Status.virtio_net_supported = false;
    g_Status.wifi_supported = false;
    g_Status.pci_device_count = 0;
    g_Status.usb_controller_count = 0;
    g_Status.ahci_controller_count = 0;
    g_Status.nvme_controller_count = 0;
    g_Status.network_controller_count = 0;
    g_Status.ethernet_controller_count = 0;
    g_Status.wifi_controller_count = 0;
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
    g_Status.ps2_packet_decoder_ready = RunPs2PacketDecoderSelfTest();
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
    KernelLog(g_Status.ps2_packet_decoder_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.ps2_packet_decoder_ready ? "PS/2 packet decoder ready" : "PS/2 packet decoder unavailable");

    KernelLog(g_Status.pci_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.pci_ready ? "PCI bus enumerated" : "PCI bus has no visible devices");
    KernelLog(g_Status.pci_config_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.pci_config_ready ? "PCI configuration access ready" : "PCI configuration access unavailable");
    KernelLog(g_Status.pci_interrupts_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.pci_interrupts_ready ? "PCI interrupt metadata discovered" : "PCI interrupt metadata unavailable");

    KernelLog(g_Status.usb_supported ? LogLevel::Info : LogLevel::Warn,
        g_Status.usb_supported ? "USB controller support active" : "USB controller not found");
    KernelLog(g_Status.usb_hid_supported ? LogLevel::Info : LogLevel::Warn,
        g_Status.usb_hid_supported ? "USB HID parser ready" : "USB HID parser waiting for controller");
    KernelLog(g_Status.ahci_supported ? LogLevel::Info : LogLevel::Warn,
        g_Status.ahci_supported ? "AHCI controller support active" : "AHCI controller not found");
    KernelLog(g_Status.nvme_supported ? LogLevel::Info : LogLevel::Warn,
        g_Status.nvme_supported ? "NVMe controller support active" : "NVMe controller not found");
    KernelLog(g_Status.network_supported ? LogLevel::Info : LogLevel::Warn,
        g_Status.network_supported ? "Network controller support active" : "Network controller not found");
    KernelLog(g_Status.ethernet_supported ? LogLevel::Info : LogLevel::Warn,
        g_Status.ethernet_supported ? "Ethernet controller support active" : "Ethernet controller not found");
    KernelLog(g_Status.e1000_supported ? LogLevel::Info : LogLevel::Warn,
        g_Status.e1000_supported ? "Intel E1000 support active" : "Intel E1000 not found");
    KernelLog(g_Status.virtio_net_supported ? LogLevel::Info : LogLevel::Warn,
        g_Status.virtio_net_supported ? "VirtIO Net support active" : "VirtIO Net not found");
    KernelLog(g_Status.wifi_supported ? LogLevel::Info : LogLevel::Warn,
        g_Status.wifi_supported ? "Wi-Fi controller support active" : "Wi-Fi controller not found");
}
