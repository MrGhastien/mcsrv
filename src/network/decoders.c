#include "decoders.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

void pkt_decode_dummy(Packet* packet, const Connection* conn, u8* raw) {
    (void)packet;
    (void)conn;
    (void)raw;
}

void pkt_decode_handshake(Packet* packet, const Connection* conn, u8* raw) {
    PacketHandshake* hshake = malloc(sizeof(PacketHandshake));
    if (!hshake)
        return;

    size_t i = 0;
    i += decode_varint(&raw[i], &hshake->protocol_version);
    //TODO: FIX MEMORY LEAK!
    i += decode_string(&raw[i], &hshake->srv_addr);
    i += decode_u16(&raw[i], &hshake->srv_port);
    i += decode_varint(&raw[i], &hshake->next_state);

    if (i != packet->payload_length) {
        fprintf(stderr, "Mismatched read (%zu) vs expected (%zu) data size.\n", i, packet->payload_length);
        packet->payload = NULL;
        free(hshake);
        return;
    }

    packet->payload = hshake;
}
