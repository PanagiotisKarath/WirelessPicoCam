#include <stdio.h>
#include <string.h>
#include "pico/util/queue.h"
#include "wireless.h"
#include "lwip/udp.h"
#include "fs_utils.h"
#include "arducam/arducam.h"

#include <inttypes.h>

/*
 * These 2 macros define how the image is split up.
 * 81 * 1296 = 104,976 bytes which is the 324x324 image size.
*/
#define TOTAL_PACKETS 81
#define IMAGE_CHUNK 1296

/*
 * Extern variables, defined in other files
*/
extern struct arducam_config config;
extern uint8_t image_buf[324*324];
extern const int port;
extern queue_t receive_queue;
extern queue_t pbuf_queue;
extern ip4_addr_t *station_address;

// Variable that is written in queue when message is received
uint8_t message_received_value = 255;

void send_message(const ip_addr_t* remote_address, const char* message) {
    /*
     * Create packet buffer and copy the message to its payload
    */
    int message_length = strlen(message) + 1;
    struct pbuf* p = pbuf_alloc(PBUF_TRANSPORT, message_length, PBUF_RAM);
    memcpy(p->payload, message, message_length);

    /*
     * Create a udp pcb and send message to remote address. Error check and 
     * if all is well free pbuf and remove the udp pcb.
    */
    struct udp_pcb* udp = udp_new();
    err_t err = udp_sendto(udp, p, remote_address, port);
    if (err != ERR_OK) {
        printf("ERROR: message could not be sent (%d)\n", err);
    }
    pbuf_free(p);
    udp_remove(udp);
}

void send_image(const ip_addr_t* source_address, uint8_t* image) {
    uint8_t package_number = 0;

    // Create udp pcb, bind it to port and connect to access point's IP
    struct udp_pcb *pcb = udp_new();
    udp_bind(pcb, IP_ADDR_ANY, port);
    udp_connect(pcb, source_address, port);

    // The buffer stores temporarily the image data, plus the package's number
    static uint8_t packet_buffer[1 + IMAGE_CHUNK] = {0};

    while (package_number <= 80) {
        packet_buffer[0] = package_number;
        memcpy(&packet_buffer[1], image + package_number * IMAGE_CHUNK, IMAGE_CHUNK);
        
        /*
         * Allocate 2 buffer that hold the same information. Each packet is 
         * sent twice, to make up for the unreliability of udp protocol.
         * Error check the allocation of both pbufs, if no error is returned
         * copy the image data to their payload.
        */
        struct pbuf *p1 = pbuf_alloc(PBUF_TRANSPORT, sizeof(packet_buffer), PBUF_RAM);
        struct pbuf *p2 = pbuf_alloc(PBUF_TRANSPORT, sizeof(packet_buffer), PBUF_RAM);
        if (!p1) {
            printf("ERROR: referenced pbuf could not be allocated");
            pbuf_free(p2);
            return;
        }
        if (!p2) {
            printf("ERROR: referenced pbuf could not be allocated");
            pbuf_free(p1);
            return;
        }
        memcpy(p1->payload, packet_buffer, sizeof(packet_buffer));
        memcpy(p2->payload, packet_buffer, sizeof(packet_buffer));

        /*
         * Send both pbufs with a 25ms sleep between them
        */
        err_t err = udp_send(pcb, p1);
        if (err != ERR_OK) {
            printf("ERROR: message could not be sent (%d)\n", err);
        } 
        sleep_ms(25);
        err = udp_send(pcb, p2);
        if (err != ERR_OK) {
            printf("ERROR: message could not be sent (%d)\n", err);
        }

        /*
         * Increase the package_number variable that controls the while loop
         * and free packet buffers.
        */
        package_number++;
        pbuf_free(p1);
        pbuf_free(p2);
        
        // Small delay to help the receiver keep up with the packets.
        sleep_ms(100);
    }
    // Remove udp pcb.
    udp_remove(pcb);
}

void ap_udp_recv_fn(void* arg, struct udp_pcb* recv_pcb, struct pbuf* p, const ip_addr_t* source_addr, u16_t source_port) {
    /*
     * Extract the payload from the packet buffer.
    */
    char received_message[p->tot_len + 1];
    memcpy(received_message, p->payload, p->tot_len);
    received_message[p->tot_len] = '\0';
    bool element_add_success;
    int retries = 0;

    /*
     * If message is the expected, try to add an element to the queue.
     * This queue is responsible for notifying the other core to capture an 
     * image. It tries to add an element up to 3 times.
    */
    if (strcmp(received_message, "capture image") == 0) {
        element_add_success = queue_try_add(&receive_queue, &message_received_value);
        while (element_add_success == false && retries < 3) {
            retries++;
            element_add_success = queue_try_add(&receive_queue, &message_received_value);
        }

        if (retries > 3) {
            printf("ERROR: could not add element to receive queue");
        }
    }
    // free pbuf and "save" the station IP address
    pbuf_free(p);
    station_address = source_addr;
}

void sta_udp_recv_fn(void* arg, struct udp_pcb* recv_pcb, struct pbuf* p, const ip_addr_t* source_addr, u16_t source_port) {
    /*
     * Station callback is as light as possible. It only tries to add a pbuf
     * to the queue.
    */
    if (!queue_try_add(&pbuf_queue, &p)){
        printf("ERROR: could not add pbuf in queue", p->payload);
        pbuf_free(p);
        return;
    }
}
