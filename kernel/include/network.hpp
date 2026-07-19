#pragma once

#include <stdint.h>

struct NetworkStatus {
    bool ethernet_ready;
    bool arp_ready;
    bool ipv4_ready;
    bool icmp_ready;
    bool udp_ready;
    bool tcp_ready;
    bool dhcp_ready;
    bool dns_ready;
    bool http_ready;
    bool https_ready;
    bool socket_api_ready;
    bool ipv6_ready;
    bool tls_ready;
    bool websocket_ready;
    bool mdns_ready;
    bool ntp_ready;
    uint32_t socket_count;
};

bool KernelNetworkInit();
const NetworkStatus& KernelNetworkStatus();
void PrintNetworkInfo();
