#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "dhcpserver.h"
#include "filesystem/vfs.h"
#include "camera.h"
#include "fs_utils.h"
#include "wireless.h"
#include "arducam/arducam.h"

const int port = WIFI_PORT;

struct arducam_config config;

int main() {
    // Initialize all standard I/O interfaces
    stdio_init_all();
    // Initialize the filesystem
    fs_init();
    // Initialize the hm01b0 camera
    hm01b0_setup();

    // Initialize wireless chip with country and check for errors
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_GREECE)) {
        printf("ERROR: failed to initialize CYW43 chip.\n");
        return 1;
    }

    // WIFI credentials and set operation to access point mode
    const char *ap_name = WIFI_SSID;
    const char *password = PASSWORD;
    cyw43_arch_enable_ap_mode(ap_name, password, CYW43_AUTH_WPA2_AES_PSK);

    // Gateway IP and Subnet Mask to create DHCP Server
    ip4_addr_t gateway, netmask;
    IP4_ADDR(&gateway, 192, 168, 4, 1);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    dhcp_server_t dhcp_server;
    dhcp_server_init(&dhcp_server, &gateway, &netmask);

    // Set up UDP module
    udp_init();
    struct udp_pcb *udp = udp_new();
    if (!udp) {
        printf("ERROR: failed to create udp\n");
    }
    if (ERR_OK != udp_bind(udp, IP4_ADDR_ANY, port)) {
        printf("ERROR: failed to bind to port %u", port);
    }

    udp_recv(udp, ap_udp_recv_fn, (void*)NULL);

    while(true) {
        cyw43_arch_poll();
        sleep_ms(1);
    }

    return 0;
}