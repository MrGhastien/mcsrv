#include "decoders.h"
#include "containers/bytebuffer.h"
#include "memory/arena.h"
#include "packet.h"


PKT_DECODER(dummy) {
    (void) packet;
    (void) arena;
    (void) bytes;
}

PKT_DECODER(handshake) {
    PacketHandshake* hshake = arena_allocate(arena, sizeof(PacketHandshake));
    if (!hshake)
        return;

    if (bytebuf_read_varint(bytes, &hshake->protocol_version) <= 0) {
        return;
    }
    if (bytebuf_read_mcstring(bytes, arena, &hshake->srv_addr) <= 0) {
        return;
    }
    if (bytebuf_read(bytes, sizeof(u16), &hshake->srv_port) <= 0) {
        return;
    }
    hshake->srv_port = htons(hshake->srv_port);
    if (!bytebuf_read_varint(bytes, &hshake->next_state)) {
        return;
    }

    packet->payload = hshake;
}

PKT_DECODER(ping) {
    PacketPing* ping = arena_allocate(arena, sizeof *ping);

    // There is no need to care about endianess, as we don't do operations or comparisons
    // on the received number.
    bytebuf_read(bytes, sizeof(u64), &ping->num);
    packet->payload = ping;
}

PKT_DECODER(log_start) {
    PacketLoginStart* payload = arena_allocate(arena, sizeof *payload);
    bytebuf_read_mcstring(bytes, arena, &payload->player_name);
    bytebuf_read(bytes, 16, &payload->uuid);

    packet->payload = payload;
}

PKT_DECODER(enc_res) {
    PacketEncRes* payload = arena_allocate(arena, sizeof *payload);

    bytebuf_read_varint(bytes, &payload->shared_secret_length);
    payload->shared_secret = arena_allocate(arena, payload->shared_secret_length);
    bytebuf_read(bytes, payload->shared_secret_length, payload->shared_secret);

    bytebuf_read_varint(bytes, &payload->verify_token_length);
    payload->verify_token = arena_allocate(arena, payload->verify_token_length);
    bytebuf_read(bytes, payload->verify_token_length, payload->verify_token);

    packet->payload = payload;
}
