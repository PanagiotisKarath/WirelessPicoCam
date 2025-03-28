#include <stdio.h>
#include <string.h>
#include "wireless.h"
#include "lwip/udp.h"
#include "fs_utils.h"
#include "arducam/arducam.h"

#define TOTAL_PACKETS 81
#define IMAGE_CHUNK 1296
#define ACK_TIMEOUT_US 3000

extern struct arducam_config config;
extern uint8_t image_buf[324*324];
extern const int port;

uint16_t total_packages_received;
volatile bool ack_received = false;
volatile uint16_t packet_received_num = 0;

void send_message(const ip_addr_t* remote_address, const char* message) {
    printf("Sending Message\n"); // DEBUG

    // Create packet buffer and give it the message to "carry"
    int message_length = strlen(message) + 1;
    struct pbuf* p = pbuf_alloc(PBUF_TRANSPORT, message_length, PBUF_RAM);
    memcpy(p->payload, message, message_length);

    // Create udp pcb, bind it to port, connect to access point's IP, send the
    // message and check for potential errors
    struct udp_pcb* pcb = udp_new();
    udp_bind(pcb, IP_ADDR_ANY, port);
    udp_connect(pcb, remote_address, port);
    int err = udp_send(pcb, p);
    if (err != ERR_OK) {
        printf("ERROR: message could not be sent (%d)\n", err);
    }

    // Deallocate the memory used for the pbuf and the udp pcb
    pbuf_free(p);
    udp_remove(pcb);
}

void send_image(const ip_addr_t* source_address, uint8_t* image) {
    printf("Sending image\n"); // DEBUG
    // Offset will be increased every time a message is sent, until it's larger
    // than the whole image. image_chunk is chosen as 1296 so that every 
    // message contains 4 rows/columns of the whole image
    uint8_t package_number = 0;
    uint32_t offset = 0;
    int retries = 0;

    // Create udp pcb, bind it to port and connect to access point's IP
    struct udp_pcb *pcb = udp_new();
    udp_bind(pcb, IP_ADDR_ANY, port);
    udp_connect(pcb, source_address, port);

    // The buffer stores temporarily the image data, plus the package's number
    uint8_t packet_buffer[1 + IMAGE_CHUNK];

    while (offset < sizeof(image_buf)) {
        package_number++;
        packet_buffer[0] = package_number;
        memcpy(&packet_buffer[1], image + offset, IMAGE_CHUNK);

        // Reference the packet buffer. Referencing memory and not using 
        // pbuf_alloc saves memory space.
        struct pbuf *p = pbuf_alloc_reference(packet_buffer, sizeof(packet_buffer), PBUF_ROM);
        if (!p) {
            printf("ERROR: referenced pbuf could not be allocated");
            return;
        }

        retries = 0;
        ack_received = false;

        do {
            int err = udp_send(pcb, p);
            if (err != ERR_OK) {
                printf("ERROR: message could not be sent (%d)\n", err);
            }
            printf("Sent package number %d\n", package_number); // DEBUG

            uint64_t start_time = time_us_64();
            while (!ack_received && (time_us_64() - start_time) < ACK_TIMEOUT_US) {
                cyw43_arch_poll();
            }

            if (!ack_received) {
                printf("No ACK received for packet %d, retrying (%d)\n", package_number, retries);
            }
        } while (!ack_received && ++retries < 2); // Max 3 retries for each packet

        if (!ack_received) {
            printf("WARNING: packet %d lost\n", package_number);
        } 

        // Small delay to help the receiver keep up with the packets
        sleep_ms(50);
        // Free the pbuf after sending
        pbuf_free(p);
        // Increment the offset by the size of one image chunk
        offset += IMAGE_CHUNK;
    }
    udp_remove(pcb);
}

void ap_udp_recv_fn(void* arg, struct udp_pcb* recv_pcb, struct pbuf* p, const ip_addr_t* source_addr, u16_t source_port) {
    printf("Message received\n"); // DEBUG

    // Extract message from packet buffer, store it in array
    char received_message[p->tot_len];
    memcpy(received_message, p->payload, p->tot_len);

    // If message is "capture image", take picture, save it and send it back 
    // to the station
    if (strcmp(received_message, "capture image") == 0) {
        printf("CAPTURING IMAGE\n"); // DEBUG
        arducam_capture_frame(&config);
        save_image_sd(image_buf); //MYTODO: Change to flash
        send_image(source_addr, image_buf);
    }

    // If message is "ack", change ack_received to true
    if (strcmp(received_message, "ack") == 0) {
        printf("acknowledge message received\n"); // DEBUG
        ack_received = true;
    }

    pbuf_free(p);
}

void sta_udp_recv_fn(void* arg, struct udp_pcb* recv_pcb, struct pbuf* p, const ip_addr_t* source_addr, u16_t source_port) {
    //Send acknowledge message
    printf("Message received, sending acknowledgment\n");
    send_message(source_addr, "ack");
    
    memcpy(&packet_received_num, p->payload, sizeof(uint8_t));
    if (packet_received_num < TOTAL_PACKETS) {
        memcpy(&image_buf[(packet_received_num - 1) * IMAGE_CHUNK], p->payload + sizeof(uint8_t), IMAGE_CHUNK);
        total_packages_received++;
    }
    pbuf_free(p);
    printf("Received packet (%d)\n", packet_received_num);

    if (packet_received_num == TOTAL_PACKETS) {
        printf("SAVING IMAGE\n");
        save_image_sd(image_buf);
        printf("WARNING: Missed %d packets\n", TOTAL_PACKETS - total_packages_received - 1);
    }
}
