#ifndef WIRELESS_H
#define WIRELESS_H

#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

/*
 * Sends initial message to trigger the reading of the temperature. Called by 
 * the Station.
 *
 * @param remote_address IP Address to send the message.
 * @param message Character string to be sent.
*/
void send_message (const ip_addr_t* remote_address, const char* message);

/*
 * Sends image back to the station.
 * @param source_address IP address to send image to.
 * @param image Pointer to first element of the image array
*/
void send_image(const ip_addr_t* source_address, uint8_t* image);

/*
 * Access Point callback function. Is called automatically when the access 
 * point receives a message.
 * 
 * @param arg User supplied argument.
 * @param recv_pcb The udp pcb which received the data.
 * @param p The packet buffer received.
 * @param source_addr The address the message was sent from.
 * @param source port The port from which the packet was received
*/
void ap_udp_recv_fn(void* arg, struct udp_pcb* recv_pcb, struct pbuf* p, const ip_addr_t* source_addr, u16_t source_port);

/*
 * Station callback function. Is called automatically when the station receives 
 * a message.
 * 
 * @param arg User supplied argument.
 * @param recv_pcb The udp pcb which received the data.
 * @param p The packet buffer received.
 * @param source_addr The address the message was sent from.
 * @param source port The port from which the packet was received
*/
void sta_udp_recv_fn(void* arg, struct udp_pcb* recv_pcb, struct pbuf* p, const ip_addr_t* source_addr, u16_t source_port);

#endif