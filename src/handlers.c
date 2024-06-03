#include <stdio.h>
#include "packet.h"

void pkt_handle_handshake(Packet* pkt, Connection* conn) {
        PacketHandshake* shake = pkt->decoded;
        printf("Protocol version: %i\nServer address: '%s'", shake->protocol_version, shake->srv_addr);
        printf("\nPort: %u\nNext state: %i\n", shake->srv_port, shake->next_state);
}

void pkt_handle_status(Packet* pkt, Connection* conn) {
    
}

void pkt_handle_ping(Packet* pkt, Connection* conn) {
    
}
