#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/util/queue.h"
#include "lwip/pbuf.h"
#include "dhcpserver.h"
#include "filesystem/vfs.h"
#include "camera.h"
#include "fs_utils.h"
#include "wireless.h"
#include "arducam/arducam.h"

const int port = WIFI_PORT;
queue_t receive_queue, image_queue;
ip4_addr_t *station_address;
uint8_t image_buf[324*324];
struct arducam_config config;

void core1_entry() {
    // Initialize the filesystem
    fs_init();
    // Initialize the hm01b0 camera
    hm01b0_setup();

    uint8_t received_value;
    /*
     * Initialize 2 queues, one for communicating the receival of a message, 
     * one for passing the start of the image from one core to the other.
    */
    queue_init_with_spinlock(&receive_queue, sizeof(uint8_t), 8, 20);
    queue_init_with_spinlock(&image_queue, sizeof(uint8_t*), 1, 21);

    // Infinite while loop
    while(true) {
        // Wait for element in receive queue
        queue_remove_blocking(&receive_queue, &received_value);
        if (received_value == 255) {
            arducam_capture_frame(&config);
            save_image_flash((void*)image_buf);
            uint8_t *buf_ptr = image_buf;
            queue_add_blocking(&image_queue, &buf_ptr);
        }
    }
}

int main() {
    // Initialize all standard I/O interfaces
    stdio_init_all();
    // Launch core 1
    multicore_launch_core1(core1_entry);

    /*
     * Initialize wireless functionality for the access point.
    */
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_GREECE)) {
        printf("ERROR: failed to initialize CYW43 chip.\n");
        return 1;
    }

    const char *ap_name = WIFI_SSID;
    const char *password = PASSWORD;
    cyw43_arch_enable_ap_mode(ap_name, password, CYW43_AUTH_WPA2_AES_PSK);

    ip4_addr_t gateway, netmask;
    IP4_ADDR(&gateway, 192, 168, 4, 1);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    dhcp_server_t dhcp_server;
    dhcp_server_init(&dhcp_server, &gateway, &netmask);

    udp_init();
    struct udp_pcb *udp = udp_new();
    if (!udp) {
        printf("ERROR: failed to create udp\n");
    }
    if (ERR_OK != udp_bind(udp, IP4_ADDR_ANY, port)) {
        printf("ERROR: failed to bind to port %u", port);
    }
    udp_recv(udp, ap_udp_recv_fn, (void*)NULL);

    // Auxiliary variables
    uint8_t *image_pointer;
    bool remove_success = false;

    // Infinite while loop
    while(true) {
        cyw43_arch_poll();
        /*
         * When it core 0 receives the start of the image in this mailbox 
         * queue it sends the whole image to the station.
        */
        remove_success = queue_try_remove(&image_queue, &image_pointer);
        if (remove_success == true) {
            send_image(station_address, image_pointer);
            remove_success = false;
        }
        sleep_ms(1);
    }

    return 0;
}