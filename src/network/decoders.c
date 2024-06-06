#include "decoders.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

void pkt_decode_dummy(Packet* packet, Connection* conn, u8* raw) {
    (void)packet;
    (void)conn;
    (void)raw;
}

void pkt_decode_handshake(Packet* packet, Connection* conn, u8* raw) {
    PacketHandshake* hshake = arena_allocate(&conn->arena, sizeof(PacketHandshake));
    if (!hshake)
        return;

    size_t i = 0;
    i += decode_varint(&raw[i], &hshake->protocol_version);
    //TODO: FIX MEMORY LEAK!
    i += decode_string(&raw[i], &conn->arena, &hshake->srv_addr);
    i += decode_u16(&raw[i], &hshake->srv_port);
    i += decode_varint(&raw[i], &hshake->next_state);

    if (i != packet->payload_length) {
        fprintf(stderr, "Mismatched read (%zu) vs expected (%zu) data size.\n", i, packet->payload_length);
        packet->payload = NULL;
        arena_free_ptr(&conn->arena, hshake);
        return;
    }

    packet->payload = hshake;
}
