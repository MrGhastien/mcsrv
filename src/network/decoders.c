#include "decoders.h"
#include "logger.h"
#include "memory/arena.h"
#include "packet.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

void pkt_decode_dummy(Packet* packet, Arena* arena, u8* raw) {
    (void) packet;
    (void) arena;
    (void) raw;
}

static bool has_read_all_bytes(Packet* pkt, u64 read) {
    if (read == pkt->payload_length)
        return TRUE;

    log_errorf("Mismatched read (%zu) vs expected (%zu) data size.", read, pkt->payload_length);
    pkt->payload = NULL;
    abort();
    return FALSE;
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

    if (!has_read_all_bytes(packet, i))
        return;

    packet->payload = hshake;
}

void pkt_decode_ping(Packet* packet, Arena* arena, u8* raw) {
    PacketPing* ping = arena_allocate(arena, sizeof *ping);

    ping->num = ((u64*) raw)[0];
    packet->payload = ping;
}

void pkt_decode_log_start(Packet* packet, Arena* arena, u8* raw) {
    PacketLoginStart* payload = arena_allocate(arena, sizeof *payload);
    u64 i = decode_string(raw, arena, &payload->player_name);
    i += decode_uuid(raw, payload->uuid);

    if (!has_read_all_bytes(packet, i))
        return;

    packet->payload = payload;
}
