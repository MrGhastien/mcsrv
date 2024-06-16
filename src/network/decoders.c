#include "decoders.h"
#include "packet.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <byteswap.h>

void pkt_decode_dummy(Packet* packet, Arena* arena, u8* raw) {
    (void)packet;
    (void)arena;
    (void)raw;
}

void pkt_decode_handshake(Packet* packet, Arena* arena, u8* raw) {
    PacketHandshake* hshake = arena_allocate(arena, sizeof(PacketHandshake));
    if (!hshake)
        return;

    size_t i = 0;
    i += decode_varint(&raw[i], &hshake->protocol_version);
    i += decode_string(&raw[i], arena, &hshake->srv_addr);
    i += decode_u16(&raw[i], &hshake->srv_port);
    i += decode_varint(&raw[i], &hshake->next_state);

    if (i != packet->payload_length) {
        fprintf(stderr, "Mismatched read (%zu) vs expected (%zu) data size.\n", i, packet->payload_length);
        packet->payload = NULL;
        arena_free_ptr(arena, hshake);
        return;
    }

    packet->payload = hshake;
}

void pkt_decode_ping(Packet *packet, Arena *arena, u8 *raw) {
    PacketPing* ping = arena_allocate(arena, sizeof *ping);

    ping->num = ((u64*)raw)[0];
    packet->payload = ping;
}
