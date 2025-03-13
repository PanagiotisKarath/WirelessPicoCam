#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "wireless.h"
#include "filesystem/vfs.h"

#define BUTTON_PIN 15

volatile bool message_button_pressed = false;

const int port = WIFI_PORT;

void gpio_callback(uint gpio, uint32_t events) {
    message_button_pressed = true;
}

int main() {
    // Initialize all standard I/O interfaces
    stdio_init_all();
    // Initialize the filesystem
    fs_init();

    // Set up the button that handles sending messages
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
    gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    // Access point IP, where the station connects to
    ip4_addr_t remote_address;
    IP4_ADDR(&remote_address, 192, 168, 4, 1);

    // Initialize wireless chip with country and check for errors
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_GREECE)) {
        printf("ERROR: failed to initialize CYW43 chip\n");
        return 1;
    }

    // Enable station mode
    cyw43_arch_enable_sta_mode();

    // Get WIFI credentials from CMakeLists.txt
    const char *ap_name = WIFI_SSID;
    const char *password = PASSWORD;

    // Attempt to connect to access point's wifi network. Timeout after 30 secs
    if(cyw43_arch_wifi_connect_timeout_ms(ap_name, password, CYW43_AUTH_WPA2_AES_PSK, 30000) != 0){
        printf("ERROR: failed to connect to access point\n");
        return 1;
    }
    printf("CONNECTED TO ACCESS POINT\n");

    // Initialise udp module and receive function for the station. Also check 
    // for errors.
    udp_init();
    struct udp_pcb *udp =udp_new();
    if(!udp) {
        printf("ERROR: failed to create udp\n");
    }
    if (ERR_OK != udp_bind(udp, IP4_ADDR_ANY, port)) {
        printf("ERROR: failed to bind to port %u\n", port);
    }
    
    udp_recv(udp, sta_udp_recv_fn, (void*)NULL);

    while (true) {
        if (message_button_pressed) {
            sleep_ms(250); //Debounce time
            send_message(&remote_address, "capture image");
            message_button_pressed = false;
        }

        // Poll the cyw43 architecture to process any pending events.
        // Must be called regularly.
        sleep_ms(100); 
        cyw43_arch_poll();
    }

    return 0;
}