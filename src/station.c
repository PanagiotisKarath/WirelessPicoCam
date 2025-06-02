#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/util/queue.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "wireless.h"
#include "filesystem/vfs.h"
#include "fs_utils.h"

#define BUTTON_PIN 6

queue_t button_queue, pbuf_queue;
volatile bool message_button_pressed = false;
const int port = WIFI_PORT;
uint8_t image_buf[324*324];

void gpio_callback(uint gpio, uint32_t events) {
    message_button_pressed = true;
}

void core1_entry() {
    /*
     * Set up wireless functionality for the station
    */
    ip4_addr_t access_point_address;
    IP4_ADDR(&access_point_address, 192, 168, 4, 1);

    if (cyw43_arch_init_with_country(CYW43_COUNTRY_GREECE) != 0) {
        printf("ERROR: failed to initialize cyw43 wireless chip\n");
        return;
    }

    cyw43_arch_enable_sta_mode();
    const char *ap_name = WIFI_SSID;
    const char *password = PASSWORD;

    if (cyw43_arch_wifi_connect_timeout_ms(ap_name, password, CYW43_AUTH_WPA2_AES_PSK, 30000) != 0) {
        printf("ERROR: timed out connecting to access point (30 secs)\n");
        return;
    }
    // When connected to the access point the on-board LED turns on
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    
    udp_init();
    struct udp_pcb *udp = udp_new();
    if (!udp) {
        printf("ERROR: failed to create udp\n");
    }
    if (ERR_OK != udp_bind(udp, IP4_ADDR_ANY, port)) {
        printf("ERROR: failed to bind to port %u", port);
    }
    udp_recv(udp, sta_udp_recv_fn, (void*)NULL);

    uint8_t received_value;
    // Infinite while loop
    while(true) {
        /*
         * Blocked until button is pressed. When pressed send message to
         * access point with string "capture image".
        */
        queue_remove_blocking(&button_queue, &received_value);
        if(received_value == 255) {
            send_message(&access_point_address, "capture image");
        }
    }
}

int main() {
    // Initialize all standard interfaces
    stdio_init_all();
    // Initialize the filesystem
    fs_init();

    /*
     * Initialize 2 queues, one for communicating the press of a button, one 
     * for passing pbufs from one core to the other.
    */
    queue_init_with_spinlock(&button_queue, sizeof(uint8_t), 16, 20);
    queue_init_with_spinlock(&pbuf_queue, sizeof(struct pbuf *), 81, 21);

    // Launch core 1
    multicore_launch_core1(core1_entry);

    /*
     * Initialize button press detect interrupt
    */
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
    gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    /*
     * Auxiliary variables
    */
    const uint8_t button_pressed_value = 255;
    bool remove_success = false;
    struct pbuf *p;
    uint8_t package_number = 0;
    uint8_t *payload_ptr;
    bool image_saved = false;

    /*
     * Infinite while loop
    */
    while(true) {
        // Check if button pressed flag is raised
        if (message_button_pressed) {
            sleep_ms(200);
            queue_add_blocking(&button_queue, &button_pressed_value);
            message_button_pressed = false;
            image_saved = false;
        }
        // Check if a pbuf is received in the queue
        remove_success = queue_try_remove(&pbuf_queue, &p);
        if (remove_success == true) {
            payload_ptr = p->payload;
            memcpy(&package_number, payload_ptr, sizeof(uint8_t));
            memcpy(&image_buf[package_number * 1296], payload_ptr + sizeof(uint8_t), 1296);
            pbuf_free(p);
            if (package_number == 80 && image_saved == false) {
                image_saved = true;
                save_image_sd(image_buf);
                package_number = 0;
            }
        }
        cyw43_arch_poll();
        sleep_ms(1);
    }

    return 0;
}