#include <stdio.h>
#include <stdlib.h>
#include "packet.h"

void pkt_handle_handshake(Packet* pkt, Connection* conn) {
        PacketHandshake* shake = pkt->payload;
        printf("Protocol version: %i\nServer address: '%s'", shake->protocol_version, shake->srv_addr.base);
        printf("\nPort: %u\nNext state: %i\n", shake->srv_port, shake->next_state);
        conn->state = shake->next_state;
}

void pkt_handle_status(Packet* pkt, Connection* conn) {
    (void)pkt;
    (void)conn;
    printf("Status request\n");
}

void pkt_handle_ping(Packet* pkt, Connection* conn) {
    (void)pkt;
    (void)conn;
}
