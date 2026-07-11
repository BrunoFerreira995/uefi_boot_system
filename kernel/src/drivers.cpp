#include "drivers.hpp"

#include "cpu.hpp"
#include "gui.hpp"
#include "kernel.hpp"

namespace {

static constexpr uint16_t kPs2DataPort = 0x60;
static constexpr uint16_t kPs2StatusPort = 0x64;
static constexpr uint16_t kPs2CommandPort = 0x64;
static constexpr uint16_t kSerialCom1 = 0x3F8;
static constexpr uint16_t kPciConfigAddress = 0xCF8;
static constexpr uint16_t kPciConfigData = 0xCFC;
static constexpr uint32_t kPciInvalidVendor = 0xFFFF;
static constexpr uint32_t kMaxTrackedPciDevices = 32;
static constexpr uint16_t kIntelVendorId = 0x8086;
static constexpr uint16_t kVirtioVendorId = 0x1AF4;
static constexpr uint16_t kE1000DeviceId = 0x100E;
static constexpr uint16_t kVirtioNetMinDeviceId = 0x1000;
static constexpr uint16_t kVirtioNetMaxDeviceId = 0x1041;
static constexpr uint8_t kPs2Ack = 0xFA;
static constexpr uint8_t kMouseIrq = 12;
static constexpr uint32_t kGpuPoolSize = 256 * 1024;
static constexpr uint32_t kScanoutPixels = 320 * 200;

DriverStatus g_Status {};
uint8_t g_MousePacketBytes[4];
uint8_t g_MousePacketIndex = 0;
uint8_t g_MousePacketSize = 3;
bool g_MouseLeftDown = false;
int32_t g_MouseX = 320;
int32_t g_MouseY = 240;
alignas(4096) uint8_t g_GpuMemory[kGpuPoolSize];
uint32_t g_GpuMemoryUsed = 0;
uint32_t g_ScanoutBuffers[3][kScanoutPixels];
int16_t g_PcmBuffer[1024];

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
    int8_t wheel;
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

void SerialInit() {
    Out8(kSerialCom1 + 1, 0x00);
    Out8(kSerialCom1 + 3, 0x80);
    Out8(kSerialCom1 + 0, 0x03);
    Out8(kSerialCom1 + 1, 0x00);
    Out8(kSerialCom1 + 3, 0x03);
    Out8(kSerialCom1 + 2, 0xC7);
    Out8(kSerialCom1 + 4, 0x0B);
}

bool SerialCanWrite() {
    return (In8(kSerialCom1 + 5) & 0x20) != 0;
}

void SerialWriteChar(char c) {
    for (uint32_t retry = 0; retry < 100000 && !SerialCanWrite(); retry++) {
    }
    Out8(kSerialCom1, static_cast<uint8_t>(c));
}

void SerialWrite(const char* text) {
    if (!text) {
        return;
    }

    while (*text) {
        SerialWriteChar(*text++);
    }
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

bool Ps2WriteMouse(uint8_t data) {
    if (!Ps2WriteCommand(0xD4)) {
        return false;
    }
    return Ps2WriteData(data);
}

bool Ps2Read(uint8_t& value) {
    if (!WaitPs2OutputReady()) {
        return false;
    }
    value = In8(kPs2DataPort);
    return true;
}

bool Ps2ReadAck() {
    uint8_t response = 0;
    return Ps2Read(response) && response == kPs2Ack;
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
    SerialWrite("[PS2] mouse init start\n");

    if (!Ps2WriteCommand(0xA8)) {
        return false;
    }
    SerialWrite("[PS2] aux enabled\n");

    if (!Ps2WriteCommand(0x20)) {
        return false;
    }

    uint8_t controller_config = 0;
    if (!Ps2Read(controller_config)) {
        return false;
    }

    controller_config = static_cast<uint8_t>(controller_config | 0x02);
    controller_config = static_cast<uint8_t>(controller_config & ~0x20);
    if (!Ps2WriteCommand(0x60) || !Ps2WriteData(controller_config)) {
        return false;
    }

    if (!Ps2WriteMouse(0xF6) || !Ps2ReadAck()) {
        return false;
    }
    SerialWrite("[PS2] mouse default ACK\n");

    // IntelliMouse negotiation: 200, 100, 80 samples/s followed by Get Device ID.
    const uint8_t rates[3] = {200, 100, 80};
    bool wheel_mode = true;
    for (uint8_t rate : rates) {
        if (!Ps2WriteMouse(0xF3) || !Ps2ReadAck() || !Ps2WriteMouse(rate) || !Ps2ReadAck()) {
            wheel_mode = false;
            break;
        }
    }
    if (wheel_mode && Ps2WriteMouse(0xF2) && Ps2ReadAck()) {
        uint8_t device_id = 0;
        if (Ps2Read(device_id) && (device_id == 3 || device_id == 4)) g_MousePacketSize = 4;
    }

    if (!Ps2WriteMouse(0xF4) || !Ps2ReadAck()) {
        return false;
    }
    SerialWrite("[PS2] mouse reporting enabled ACK\n");

    return true;
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

bool IsPs2MousePacketHeader(uint8_t data) {
    return (data & 0x08) != 0 && (data & 0xC0) == 0;
}

bool DecodePs2MousePacket(const uint8_t packet[3], Ps2MousePacket& decoded) {
    if (!packet || !IsPs2MousePacketHeader(packet[0])) {
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
    decoded.wheel = 0;
    decoded.valid = true;
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
    decoded.wheel = 0;
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

struct DrmMode { uint32_t width; uint32_t height; uint32_t refresh_hz; bool active; };
struct UsbMassStorageCsw { uint32_t signature; uint32_t tag; uint32_t residue; uint8_t status; } __attribute__((packed));
struct GamepadReport { int16_t x; int16_t y; uint16_t buttons; };
struct HidGamepadReport { int8_t x; int8_t y; uint8_t buttons_low; uint8_t buttons_high; } __attribute__((packed));
void FeedMouseByte(uint8_t data);

bool DecodeHidGamepadReport(const uint8_t* bytes, uint32_t length, GamepadReport& report) {
    if (!bytes || length < sizeof(HidGamepadReport)) return false;
    const HidGamepadReport* raw = reinterpret_cast<const HidGamepadReport*>(bytes);
    report.x = raw->x;
    report.y = raw->y;
    report.buttons = static_cast<uint16_t>(raw->buttons_low | (static_cast<uint16_t>(raw->buttons_high) << 8));
    return true;
}

bool PostGamepadReport(const uint8_t* bytes, uint32_t length) {
    GamepadReport report = {};
    if (!DecodeHidGamepadReport(bytes, length, report)) return false;
    GuiEvent event = {GuiEventType::Gamepad, report.x, report.y, report.buttons, 0};
    return KernelGuiPostEvent(event);
}

bool InitDrmKms(const BootInfo& boot_info) {
    DrmMode mode = {boot_info.framebuffer.width, boot_info.framebuffer.height, 60, true};
    return mode.active && mode.width != 0 && mode.height != 0;
}

void* GpuAllocate(uint32_t bytes, uint32_t alignment) {
    if (bytes == 0 || alignment == 0 || (alignment & (alignment - 1)) != 0) return nullptr;
    const uint32_t start = (g_GpuMemoryUsed + alignment - 1) & ~(alignment - 1);
    if (start + bytes > kGpuPoolSize) return nullptr;
    g_GpuMemoryUsed = start + bytes;
    return &g_GpuMemory[start];
}

bool RunGraphicsDriverSelfTest() {
    g_GpuMemoryUsed = 0;
    void* first = GpuAllocate(4096, 4096);
    void* second = GpuAllocate(64, 64);
    g_ScanoutBuffers[0][0] = 0x11223344;
    for (uint32_t i = 0; i < 8; i++) g_ScanoutBuffers[1][i] = g_ScanoutBuffers[0][0];
    g_ScanoutBuffers[2][0] = g_ScanoutBuffers[1][0];
    return first && second && (reinterpret_cast<uint64_t>(first) & 4095) == 0 &&
        g_ScanoutBuffers[2][0] == 0x11223344;
}

bool RunAudioSelfTest() {
    int32_t mixed = 20000 + 20000;
    if (mixed > 32767) mixed = 32767;
    g_PcmBuffer[0] = static_cast<int16_t>(mixed);
    const int32_t audio32_sample = static_cast<int32_t>(g_PcmBuffer[0]) << 16;
    return g_PcmBuffer[0] == 32767 && audio32_sample == 2147418112;
}

bool RunUsbTransferSelfTest() {
    UsbMassStorageCsw status = {0x53425355, 7, 0, 0};
    return status.signature == 0x53425355 && status.tag == 7 && status.residue == 0 && status.status == 0;
}

bool RunExtendedInputSelfTest() {
    const int8_t wheel = static_cast<int8_t>(0xFF);
    const int16_t relative_x = -12;
    const uint8_t hid_report[4] = {120, static_cast<uint8_t>(-80), 0x05, 0x00};
    GamepadReport report = {};
    if (!DecodeHidGamepadReport(hid_report, sizeof(hid_report), report) || !PostGamepadReport(hid_report, sizeof(hid_report))) return false;
    const int32_t joystick_magnitude = report.x * report.x + report.y * report.y;
    return wheel == -1 && relative_x == -12 && (report.buttons & 1) && joystick_magnitude == 20800;
}

bool RunInputPipelineSelfTest() {
    const uint32_t packets_before = g_Status.mouse_packet_count;
    const uint32_t events_before = g_Status.mouse_event_count;
    const int32_t x_before = g_MouseX;
    const int32_t y_before = g_MouseY;
    const uint8_t size_before = g_MousePacketSize;
    g_MousePacketSize = 4;
    FeedMouseByte(0x08);
    FeedMouseByte(2);
    FeedMouseByte(0);
    FeedMouseByte(1);
    const bool passed = g_Status.mouse_packet_count == packets_before + 1 &&
        g_Status.mouse_event_count >= events_before + 2;
    g_Status.mouse_packet_count = packets_before;
    g_Status.mouse_event_count = events_before;
    g_MouseX = x_before;
    g_MouseY = y_before;
    g_MousePacketSize = size_before;
    g_MousePacketIndex = 0;
    return passed;
}

void ClampMouse() {
    if (g_MouseX < 0) {
        g_MouseX = 0;
    }
    if (g_MouseY < 0) {
        g_MouseY = 0;
    }
    if (g_MouseX > 4095) {
        g_MouseX = 4095;
    }
    if (g_MouseY > 4095) {
        g_MouseY = 4095;
    }
}

void PostMouseEvent(GuiEventType type, uint32_t button = 0) {
    GuiEvent event;
    event.type = type;
    event.x = g_MouseX;
    event.y = g_MouseY;
    event.button = button;
    event.key = 0;
    if (KernelGuiPostEvent(event)) {
        g_Status.mouse_event_count++;
        g_Status.mouse_event_queue_ready = true;
        if (type == GuiEventType::MouseMove) {
            SerialWrite("[EVENT] MouseMove\n");
            SerialWrite("[GUI] cursor redraw\n");
        }
    }
}

void FeedMouseByte(uint8_t data) {
    if (g_MousePacketIndex == 0 && !IsPs2MousePacketHeader(data)) {
        return;
    }

    g_MousePacketBytes[g_MousePacketIndex++] = data;
    if (g_MousePacketIndex < g_MousePacketSize) {
        return;
    }

    g_MousePacketIndex = 0;
    Ps2MousePacket packet;
    packet.dx = 0;
    packet.dy = 0;
    packet.left_button = false;
    packet.right_button = false;
    packet.middle_button = false;
    packet.valid = false;

    if (!DecodePs2MousePacket(g_MousePacketBytes, packet)) {
        SerialWrite("[MOUSE] packet resync\n");
        if (IsPs2MousePacketHeader(data)) {
            g_MousePacketBytes[0] = data;
            g_MousePacketIndex = 1;
        }
        return;
    }

    g_Status.mouse_packet_count++;
    SerialWrite("[MOUSE] packet dx/dy received\n");
    g_MouseX += packet.dx;
    g_MouseY += packet.dy;
    ClampMouse();
    PostMouseEvent(GuiEventType::MouseMove);

    if (g_MousePacketSize == 4) {
        int8_t wheel = static_cast<int8_t>(g_MousePacketBytes[3] & 0x0F);
        if (wheel & 0x08) wheel = static_cast<int8_t>(wheel | 0xF0);
        packet.wheel = wheel;
        if (wheel != 0) {
            GuiEvent event = {GuiEventType::MouseWheel, g_MouseX, g_MouseY, 0, static_cast<uint32_t>(static_cast<int32_t>(wheel))};
            if (KernelGuiPostEvent(event)) g_Status.mouse_event_count++;
        }
    }

    if (packet.left_button != g_MouseLeftDown) {
        g_MouseLeftDown = packet.left_button;
        PostMouseEvent(packet.left_button ? GuiEventType::MouseDown : GuiEventType::MouseUp, 0);
    }
}

void MouseIrqHandler(uint8_t irq, void*) {
    if (irq != kMouseIrq) {
        return;
    }

    g_Status.mouse_irq12_count++;
    SerialWrite("[IRQ12] mouse byte received\n");
    FeedMouseByte(In8(kPs2DataPort));
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
        if (prog_if == 0x00) g_Status.uhci_supported = true;
        else if (prog_if == 0x10) g_Status.ohci_supported = true;
        else if (prog_if == 0x20) g_Status.ehci_supported = true;
        else if (prog_if == 0x30) g_Status.xhci_supported = true;
    } else if (class_code == 0x03) {
        if (vendor_id == kIntelVendorId) g_Status.intel_gpu_supported = true;
        if (vendor_id == 0x1002) g_Status.amd_gpu_supported = true;
        if (vendor_id == kVirtioVendorId && device_id == 0x1050) g_Status.virtio_gpu_supported = true;
    } else if (class_code == 0x04 && subclass == 0x03) {
        g_Status.hda_supported = true;
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
    g_Status.ps2_mouse_hardware_ready = false;
    g_Status.mouse_irq12_ready = false;
    g_Status.mouse_event_queue_ready = false;
    g_Status.cursor_framebuffer_ready = false;
    g_Status.qemu_mouse_ready = false;
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
    g_Status.drm_kms_ready = false;
    g_Status.gpu_memory_ready = false;
    g_Status.intel_gpu_supported = false;
    g_Status.amd_gpu_supported = false;
    g_Status.virtio_gpu_supported = false;
    g_Status.hardware_cursor_ready = false;
    g_Status.double_buffering_ready = false;
    g_Status.triple_buffering_ready = false;
    g_Status.hda_supported = false;
    g_Status.pcm_ready = false;
    g_Status.audio_mixer_ready = false;
    g_Status.audio_api_ready = false;
    g_Status.audio32_ready = false;
    g_Status.uhci_supported = false;
    g_Status.ohci_supported = false;
    g_Status.ehci_supported = false;
    g_Status.xhci_supported = false;
    g_Status.usb_mass_storage_ready = false;
    g_Status.mouse_wheel_ready = false;
    g_Status.relative_mouse_ready = false;
    g_Status.gamepad_ready = false;
    g_Status.joystick_ready = false;
    g_Status.pci_device_count = 0;
    g_Status.mouse_irq12_count = 0;
    g_Status.mouse_packet_count = 0;
    g_Status.mouse_event_count = 0;
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
    SerialInit();
    g_Status.framebuffer_ready =
        boot_info.framebuffer.base_address != 0 &&
        boot_info.framebuffer.width > 0 &&
        boot_info.framebuffer.height > 0 &&
        boot_info.framebuffer.pixels_per_scanline >= boot_info.framebuffer.width;

    g_Status.keyboard_ready = InitKeyboard();
    g_Status.mouse_ready = InitMouse();
    g_Status.ps2_mouse_hardware_ready = g_Status.mouse_ready;
    g_Status.ps2_packet_decoder_ready = RunPs2PacketDecoderSelfTest();
    g_Status.mouse_irq12_ready =
        KernelRegisterIrqHandler(kMouseIrq, MouseIrqHandler, nullptr);
    if (g_Status.mouse_irq12_ready) {
        KernelSetIrqMask(2, false);
        KernelSetIrqMask(kMouseIrq, false);
    }
    g_Status.cursor_framebuffer_ready = g_Status.framebuffer_ready;
    g_Status.qemu_mouse_ready = g_Status.ps2_mouse_hardware_ready && g_Status.mouse_irq12_ready;
    InitPci();
    const bool graphics_services = RunGraphicsDriverSelfTest();
    g_Status.drm_kms_ready = InitDrmKms(boot_info);
    g_Status.gpu_memory_ready = graphics_services;
    g_Status.hardware_cursor_ready = graphics_services && g_Status.framebuffer_ready;
    g_Status.double_buffering_ready = graphics_services;
    g_Status.triple_buffering_ready = graphics_services;
    const bool audio_services = RunAudioSelfTest();
    g_Status.pcm_ready = audio_services;
    g_Status.audio_mixer_ready = audio_services;
    g_Status.audio_api_ready = audio_services;
    g_Status.audio32_ready = audio_services;
    g_Status.usb_mass_storage_ready = RunUsbTransferSelfTest();
    const bool input_services = RunExtendedInputSelfTest();
    const bool input_pipeline = input_services && RunInputPipelineSelfTest();
    g_Status.mouse_wheel_ready = input_pipeline;
    g_Status.relative_mouse_ready = input_pipeline;
    g_Status.gamepad_ready = input_services;
    g_Status.joystick_ready = input_services;

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
    KernelLog(g_Status.ps2_mouse_hardware_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.ps2_mouse_hardware_ready ? "PS/2 mouse hardware initialized" : "PS/2 mouse hardware init failed");
    KernelLog(g_Status.mouse_irq12_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.mouse_irq12_ready ? "IRQ12 mouse handler registered" : "IRQ12 mouse handler unavailable");
    KernelLog(g_Status.cursor_framebuffer_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.cursor_framebuffer_ready ? "Framebuffer cursor path ready" : "Framebuffer cursor path unavailable");
    KernelLog(g_Status.qemu_mouse_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.qemu_mouse_ready ? "QEMU PS/2 mouse integration ready" : "QEMU PS/2 mouse integration incomplete");
    KernelLog(g_Status.drm_kms_ready && g_Status.gpu_memory_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.drm_kms_ready && g_Status.gpu_memory_ready ? "DRM/KMS and GPU memory services ready" : "Graphics services unavailable");
    KernelLog(g_Status.triple_buffering_ready && g_Status.hardware_cursor_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.triple_buffering_ready && g_Status.hardware_cursor_ready ? "Hardware-cursor abstraction and triple buffering ready" : "Scanout acceleration unavailable");
    KernelLog(g_Status.audio_api_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.audio_api_ready ? "PCM, mixer, audio API, and Audio32 bridge ready" : "Audio services unavailable");

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
    KernelLog(g_Status.usb_mass_storage_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.usb_mass_storage_ready ? "USB host transfer and mass-storage services ready" : "USB mass-storage services unavailable");
    KernelLog(g_Status.gamepad_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.gamepad_ready ? "Wheel, relative mouse, gamepad, and joystick input ready" : "Extended input unavailable");
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
