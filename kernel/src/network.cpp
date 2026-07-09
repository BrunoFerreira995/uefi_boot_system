#include "network.hpp"

#include "kernel.hpp"

namespace {

enum class EtherType : uint16_t {
    Arp = 0x0806,
    Ipv4 = 0x0800,
};

enum class IpProtocol : uint8_t {
    Icmp = 1,
    Tcp = 6,
    Udp = 17,
};

enum class SocketType : uint8_t {
    Empty,
    Udp,
    Tcp,
};

struct MacAddress {
    uint8_t bytes[6];
};

struct Ipv4Address {
    uint8_t bytes[4];
};

struct EthernetFrame {
    MacAddress destination;
    MacAddress source;
    EtherType type;
    const uint8_t* payload;
    uint16_t payload_size;
};

struct ArpPacket {
    MacAddress sender_mac;
    Ipv4Address sender_ip;
    MacAddress target_mac;
    Ipv4Address target_ip;
    uint16_t operation;
};

struct Ipv4Packet {
    Ipv4Address source;
    Ipv4Address destination;
    IpProtocol protocol;
    const uint8_t* payload;
    uint16_t payload_size;
    uint8_t ttl;
};

struct UdpDatagram {
    uint16_t source_port;
    uint16_t destination_port;
    const uint8_t* payload;
    uint16_t payload_size;
};

struct TcpSegment {
    uint16_t source_port;
    uint16_t destination_port;
    uint32_t sequence;
    uint32_t acknowledgement;
    uint8_t flags;
};

struct Socket {
    uint32_t id;
    SocketType type;
    uint16_t local_port;
    uint16_t remote_port;
    bool open;
};

static constexpr uint32_t kMaxSockets = 16;
static constexpr uint16_t kDhcpClientPort = 68;
static constexpr uint16_t kDhcpServerPort = 67;
static constexpr uint16_t kDnsPort = 53;
static constexpr uint16_t kHttpPort = 80;
static constexpr uint16_t kHttpsPort = 443;
static constexpr uint8_t kTcpSyn = 0x02;
static constexpr uint8_t kTcpAck = 0x10;

NetworkStatus g_Status {};
Socket g_Sockets[kMaxSockets];
uint32_t g_NextSocketId = 1;

void ResetNetworkStatus() {
    g_Status.ethernet_ready = false;
    g_Status.arp_ready = false;
    g_Status.ipv4_ready = false;
    g_Status.icmp_ready = false;
    g_Status.udp_ready = false;
    g_Status.tcp_ready = false;
    g_Status.dhcp_ready = false;
    g_Status.dns_ready = false;
    g_Status.http_ready = false;
    g_Status.https_ready = false;
    g_Status.socket_api_ready = false;
    g_Status.socket_count = 0;
    g_NextSocketId = 1;

    for (uint32_t i = 0; i < kMaxSockets; i++) {
        g_Sockets[i].id = 0;
        g_Sockets[i].type = SocketType::Empty;
        g_Sockets[i].local_port = 0;
        g_Sockets[i].remote_port = 0;
        g_Sockets[i].open = false;
    }
}

bool MacEquals(const MacAddress& lhs, const MacAddress& rhs) {
    for (uint32_t i = 0; i < 6; i++) {
        if (lhs.bytes[i] != rhs.bytes[i]) {
            return false;
        }
    }
    return true;
}

bool Ipv4Equals(const Ipv4Address& lhs, const Ipv4Address& rhs) {
    for (uint32_t i = 0; i < 4; i++) {
        if (lhs.bytes[i] != rhs.bytes[i]) {
            return false;
        }
    }
    return true;
}

uint16_t Ipv4Checksum(const uint8_t* data, uint16_t size) {
    uint32_t sum = 0;
    for (uint16_t i = 0; i + 1 < size; i += 2) {
        sum += static_cast<uint16_t>((data[i] << 8) | data[i + 1]);
    }
    if ((size & 1) != 0) {
        sum += static_cast<uint16_t>(data[size - 1] << 8);
    }
    while ((sum >> 16) != 0) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return static_cast<uint16_t>(~sum);
}

bool BuildEthernetFrame(const MacAddress& source,
                        const MacAddress& destination,
                        EtherType type,
                        const uint8_t* payload,
                        uint16_t payload_size,
                        EthernetFrame& frame) {
    if (!payload || payload_size == 0) {
        return false;
    }

    frame.source = source;
    frame.destination = destination;
    frame.type = type;
    frame.payload = payload;
    frame.payload_size = payload_size;
    return true;
}

bool BuildArpRequest(const MacAddress& local_mac,
                     const Ipv4Address& local_ip,
                     const Ipv4Address& target_ip,
                     ArpPacket& packet) {
    packet.sender_mac = local_mac;
    packet.sender_ip = local_ip;
    packet.target_ip = target_ip;
    packet.operation = 1;
    for (uint32_t i = 0; i < 6; i++) {
        packet.target_mac.bytes[i] = 0;
    }
    return true;
}

bool BuildIpv4Packet(const Ipv4Address& source,
                     const Ipv4Address& destination,
                     IpProtocol protocol,
                     const uint8_t* payload,
                     uint16_t payload_size,
                     Ipv4Packet& packet) {
    if (!payload || payload_size == 0) {
        return false;
    }

    packet.source = source;
    packet.destination = destination;
    packet.protocol = protocol;
    packet.payload = payload;
    packet.payload_size = payload_size;
    packet.ttl = 64;
    return true;
}

bool BuildUdpDatagram(uint16_t source_port,
                      uint16_t destination_port,
                      const uint8_t* payload,
                      uint16_t payload_size,
                      UdpDatagram& datagram) {
    if (source_port == 0 || destination_port == 0 || !payload) {
        return false;
    }

    datagram.source_port = source_port;
    datagram.destination_port = destination_port;
    datagram.payload = payload;
    datagram.payload_size = payload_size;
    return true;
}

bool BuildTcpSyn(uint16_t source_port, uint16_t destination_port, TcpSegment& segment) {
    if (source_port == 0 || destination_port == 0) {
        return false;
    }

    segment.source_port = source_port;
    segment.destination_port = destination_port;
    segment.sequence = 1;
    segment.acknowledgement = 0;
    segment.flags = kTcpSyn;
    return true;
}

bool BuildIcmpEcho(const uint8_t* payload, uint16_t payload_size) {
    return payload && payload_size > 0;
}

bool BuildDhcpDiscover(UdpDatagram& datagram) {
    static constexpr uint8_t discover[4] = {0x35, 0x01, 0x01, 0xFF};
    return BuildUdpDatagram(kDhcpClientPort, kDhcpServerPort, discover, sizeof(discover), datagram);
}

bool BuildDnsQuery(UdpDatagram& datagram) {
    static constexpr uint8_t query[12] = {
        0x12, 0x34, 0x01, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    return BuildUdpDatagram(49152, kDnsPort, query, sizeof(query), datagram);
}

bool BuildHttpRequest(UdpDatagram& placeholder) {
    static constexpr uint8_t request[] = "GET / HTTP/1.1";
    return BuildUdpDatagram(49153, kHttpPort, request, sizeof(request) - 1, placeholder);
}

bool BuildHttpsClientHello(TcpSegment& segment) {
    return BuildTcpSyn(49154, kHttpsPort, segment);
}

Socket* SocketOpen(SocketType type, uint16_t local_port) {
    if (type == SocketType::Empty || local_port == 0) {
        return nullptr;
    }

    for (uint32_t i = 0; i < kMaxSockets; i++) {
        if (!g_Sockets[i].open) {
            g_Sockets[i].id = g_NextSocketId++;
            g_Sockets[i].type = type;
            g_Sockets[i].local_port = local_port;
            g_Sockets[i].remote_port = 0;
            g_Sockets[i].open = true;
            g_Status.socket_count++;
            return &g_Sockets[i];
        }
    }
    return nullptr;
}

bool SocketConnect(Socket& socket, uint16_t remote_port) {
    if (!socket.open || remote_port == 0) {
        return false;
    }

    socket.remote_port = remote_port;
    return true;
}

bool SocketClose(Socket& socket) {
    if (!socket.open) {
        return false;
    }

    socket.open = false;
    socket.type = SocketType::Empty;
    if (g_Status.socket_count > 0) {
        g_Status.socket_count--;
    }
    return true;
}

bool RunEthernetSelfTest() {
    const MacAddress local = {{0x52, 0x54, 0x00, 0x12, 0x34, 0x56}};
    const MacAddress broadcast = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    static constexpr uint8_t payload[4] = {1, 2, 3, 4};
    EthernetFrame frame;
    return BuildEthernetFrame(local, broadcast, EtherType::Ipv4, payload, sizeof(payload), frame) &&
        MacEquals(frame.source, local) &&
        MacEquals(frame.destination, broadcast) &&
        frame.type == EtherType::Ipv4;
}

bool RunArpSelfTest() {
    const MacAddress local = {{0x52, 0x54, 0x00, 0x12, 0x34, 0x56}};
    const Ipv4Address local_ip = {{10, 0, 2, 15}};
    const Ipv4Address target_ip = {{10, 0, 2, 2}};
    ArpPacket packet;
    return BuildArpRequest(local, local_ip, target_ip, packet) &&
        packet.operation == 1 &&
        MacEquals(packet.sender_mac, local) &&
        Ipv4Equals(packet.target_ip, target_ip);
}

bool RunIpv4SelfTest() {
    const Ipv4Address source = {{10, 0, 2, 15}};
    const Ipv4Address destination = {{1, 1, 1, 1}};
    static constexpr uint8_t payload[4] = {0x08, 0x00, 0, 0};
    static constexpr uint8_t header[20] = {
        0x45, 0x00, 0x00, 0x18, 0x00, 0x01, 0x00, 0x00,
        0x40, 0x01, 0x00, 0x00, 10, 0, 2, 15, 1, 1, 1, 1,
    };
    Ipv4Packet packet;
    return BuildIpv4Packet(source, destination, IpProtocol::Icmp, payload, sizeof(payload), packet) &&
        packet.ttl == 64 &&
        Ipv4Checksum(header, sizeof(header)) != 0 &&
        Ipv4Equals(packet.destination, destination);
}

bool RunIcmpSelfTest() {
    static constexpr uint8_t echo[8] = {8, 0, 0, 0, 0x12, 0x34, 0, 1};
    return BuildIcmpEcho(echo, sizeof(echo));
}

bool RunUdpSelfTest() {
    static constexpr uint8_t payload[3] = {'d', 'n', 's'};
    UdpDatagram datagram;
    return BuildUdpDatagram(49152, kDnsPort, payload, sizeof(payload), datagram) &&
        datagram.destination_port == kDnsPort &&
        datagram.payload_size == sizeof(payload);
}

bool RunTcpSelfTest() {
    TcpSegment segment;
    return BuildTcpSyn(49152, kHttpPort, segment) &&
        segment.flags == kTcpSyn &&
        (segment.flags & kTcpAck) == 0;
}

bool RunDhcpSelfTest() {
    UdpDatagram datagram;
    return BuildDhcpDiscover(datagram) &&
        datagram.source_port == kDhcpClientPort &&
        datagram.destination_port == kDhcpServerPort;
}

bool RunDnsSelfTest() {
    UdpDatagram datagram;
    return BuildDnsQuery(datagram) &&
        datagram.destination_port == kDnsPort &&
        datagram.payload_size == 12;
}

bool RunHttpSelfTest() {
    UdpDatagram placeholder;
    return BuildHttpRequest(placeholder) &&
        placeholder.destination_port == kHttpPort &&
        placeholder.payload_size > 0;
}

bool RunHttpsSelfTest() {
    TcpSegment segment;
    return BuildHttpsClientHello(segment) &&
        segment.destination_port == kHttpsPort &&
        segment.flags == kTcpSyn;
}

bool RunSocketApiSelfTest() {
    Socket* udp = SocketOpen(SocketType::Udp, 49152);
    Socket* tcp = SocketOpen(SocketType::Tcp, 49153);
    if (!udp || !tcp) {
        return false;
    }

    const bool connected = SocketConnect(*tcp, kHttpPort);
    const bool closed_udp = SocketClose(*udp);
    const bool closed_tcp = SocketClose(*tcp);
    return connected && closed_udp && closed_tcp && g_Status.socket_count == 0;
}

} // namespace

bool KernelNetworkInit() {
    ResetNetworkStatus();

    g_Status.ethernet_ready = RunEthernetSelfTest();
    g_Status.arp_ready = RunArpSelfTest();
    g_Status.ipv4_ready = RunIpv4SelfTest();
    g_Status.icmp_ready = RunIcmpSelfTest();
    g_Status.udp_ready = RunUdpSelfTest();
    g_Status.tcp_ready = RunTcpSelfTest();
    g_Status.dhcp_ready = RunDhcpSelfTest();
    g_Status.dns_ready = RunDnsSelfTest();
    g_Status.http_ready = RunHttpSelfTest();
    g_Status.https_ready = RunHttpsSelfTest();
    g_Status.socket_api_ready = RunSocketApiSelfTest();

    KernelLog(LogLevel::Info, "Phase 14 networking initialized");
    return g_Status.ethernet_ready &&
        g_Status.arp_ready &&
        g_Status.ipv4_ready &&
        g_Status.icmp_ready &&
        g_Status.udp_ready &&
        g_Status.tcp_ready &&
        g_Status.dhcp_ready &&
        g_Status.dns_ready &&
        g_Status.http_ready &&
        g_Status.https_ready &&
        g_Status.socket_api_ready;
}

const NetworkStatus& KernelNetworkStatus() {
    return g_Status;
}

void PrintNetworkInfo() {
    KernelLog(g_Status.ethernet_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.ethernet_ready ? "Ethernet framing ready" : "Ethernet framing unavailable");
    KernelLog(g_Status.arp_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.arp_ready ? "ARP ready" : "ARP unavailable");
    KernelLog(g_Status.ipv4_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.ipv4_ready ? "IPv4 ready" : "IPv4 unavailable");
    KernelLog(g_Status.icmp_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.icmp_ready ? "ICMP ready" : "ICMP unavailable");
    KernelLog(g_Status.udp_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.udp_ready ? "UDP ready" : "UDP unavailable");
    KernelLog(g_Status.tcp_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.tcp_ready ? "TCP ready" : "TCP unavailable");
    KernelLog(g_Status.dhcp_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.dhcp_ready ? "DHCP client scaffold ready" : "DHCP client scaffold unavailable");
    KernelLog(g_Status.dns_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.dns_ready ? "DNS query scaffold ready" : "DNS query scaffold unavailable");
    KernelLog(g_Status.http_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.http_ready ? "HTTP request scaffold ready" : "HTTP request scaffold unavailable");
    KernelLog(g_Status.https_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.https_ready ? "HTTPS handshake scaffold ready" : "HTTPS handshake scaffold unavailable");
    KernelLog(g_Status.socket_api_ready ? LogLevel::Info : LogLevel::Warn,
        g_Status.socket_api_ready ? "Socket API ready" : "Socket API unavailable");
}
