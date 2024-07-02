#include "decoders.h"
#include "logger.h"
#include "memory/arena.h"
#include "packet.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PKT_DECODER(dummy) {
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

PKT_DECODER(handshake) {
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

PKT_DECODER(ping) {
    PacketPing* ping = arena_allocate(arena, sizeof *ping);

    ping->num = ((u64*) raw)[0];
    packet->payload = ping;
}

PKT_DECODER(log_start) {
    PacketLoginStart* payload = arena_allocate(arena, sizeof *payload);
    u64 i = decode_string(raw, arena, &payload->player_name);
    i += decode_uuid(raw, payload->uuid);

    if (!has_read_all_bytes(packet, i))
        return;

    packet->payload = payload;
}

PKT_DECODER(enc_res) {
    PacketEncRes* payload = arena_allocate(arena, sizeof *payload);

    u64 i = 0;
    i += decode_varint(raw, &payload->shared_secret_length);
    payload->shared_secret = arena_allocate(arena, payload->shared_secret_length);
    memcpy(payload->shared_secret, &raw[i], payload->shared_secret_length);
    i += payload->shared_secret_length;
    i += decode_varint(raw, &payload->verify_token_length);
    payload->verify_token = arena_allocate(arena, payload->verify_token_length);
    memcpy(payload->verify_token, &raw[i], payload->verify_token_length);
    i += payload->verify_token_length;

    if (!has_read_all_bytes(packet, i)) {
        return;
    }

    packet->payload = payload;
}
