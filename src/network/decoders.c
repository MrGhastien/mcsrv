#include "decoders.h"
#include "containers/bytebuffer.h"
#include "definitions.h"
#include "memory/arena.h"
#include "memory/mem_tags.h"
#include "packet.h"
#include "logger.h"
#include "utils/bitwise.h"
#include <openssl/bio.h>

DEF_PKT_DECODER(dummy) {
    (void) packet;
    (void) arena;
    (void) bytes;
}

DEF_PKT_DECODER(handshake) {
    PacketHandshake* hshake = arena_allocate(arena, sizeof(PacketHandshake), ALLOC_TAG_PACKET);
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

DEF_PKT_DECODER(ping) {
    PacketPing* ping = arena_allocate(arena, sizeof *ping, ALLOC_TAG_PACKET);

    // There is no need to care about endianess, as we don't do operations or comparisons
    // on the received number.
    bytebuf_read(bytes, sizeof(u64), &ping->num);
    packet->payload = ping;
}

DEF_PKT_DECODER(login_start) {
    PacketLoginStart* payload = arena_allocate(arena, sizeof *payload, ALLOC_TAG_PACKET);
    bytebuf_read_mcstring(bytes, arena, &payload->player_name);
    bytebuf_read(bytes, 16, &payload->uuid);

    packet->payload = payload;
}

DEF_PKT_DECODER(crypt_response) {
    PacketCryptResponse* payload = arena_allocate(arena, sizeof *payload, ALLOC_TAG_PACKET);

    bytebuf_read_varint(bytes, &payload->shared_secret_length);
    payload->shared_secret = arena_allocate(arena, payload->shared_secret_length, ALLOC_TAG_UNKNOWN);
    bytebuf_read(bytes, payload->shared_secret_length, payload->shared_secret);

    bytebuf_read_varint(bytes, &payload->verify_token_length);
    payload->verify_token = arena_allocate(arena, payload->verify_token_length, ALLOC_TAG_UNKNOWN);
    bytebuf_read(bytes, payload->verify_token_length, payload->verify_token);

    packet->payload = payload;
}

/* === CONFIGURATION === */

DEF_PKT_DECODER(cfg_custom) {
    PacketCustom* payload = arena_callocate(arena, sizeof *payload, ALLOC_TAG_PACKET);

    u64 res = 0;

    string channel_str;
    res = bytebuf_read_mcstring(bytes, arena, &channel_str);
    if(res <= 0)
        return;

    if(!resid_parse(&channel_str, arena, &payload->channel))
        return;

    payload->data_length = packet->payload_length - res;
    if(payload->data_length > 32676) {
        log_errorf("Custom packet's payload is too large: is %zu bytes long, max 32767.", payload->data_length);
        return;
    }
    payload->data = arena_allocate(arena, payload->data_length, ALLOC_TAG_PACKET);
    bytebuf_read(bytes, payload->data_length, payload->data);

    packet->payload = payload;
}

DEF_PKT_DECODER(cfg_client_info) {
    PacketClientInfo* payload = arena_callocate(arena, sizeof *payload, ALLOC_TAG_PACKET);

    bytebuf_read_mcstring(bytes, arena, &payload->locale);
    if(payload->locale.length > 16) {
        log_error("Invalid client info packet: Locale string is longer than 16 characters.");
        return;
    }

    bytebuf_read(bytes, sizeof payload->render_distance, &payload->render_distance);

    i32 tmp;
    bytebuf_read_varint(bytes, &tmp);
    payload->chat_mode = tmp;

    bytebuf_read(bytes, sizeof payload->chat_colors, &payload->chat_colors);
    bytebuf_read(bytes, sizeof payload->skin_part_mask, &payload->skin_part_mask);
    bytebuf_read_varint(bytes, &tmp);
    payload->hand = tmp;

    bytebuf_read(bytes, sizeof payload->text_filtering, &payload->text_filtering);
    bytebuf_read(bytes, sizeof payload->allow_server_listings, &payload->allow_server_listings);
    bytebuf_read_varint(bytes, &tmp);
    payload->particle_status = tmp;

    packet->payload = payload;
}
DEF_PKT_DECODER(cfg_known_datapacks) {
    PacketKnownDatapacks* payload = arena_callocate(arena, sizeof *payload, ALLOC_TAG_PACKET);

    i32 count;
    bytebuf_read_varint(bytes, &count);

    for (i32 i = 0; i < count; ++i) {
        KnownDatapack pack;
        bytebuf_read_mcstring(bytes, arena, &pack.namespace);
        bytebuf_read_mcstring(bytes, arena, &pack.id);
        bytebuf_read_mcstring(bytes, arena, &pack.version);
        vect_add(&payload->known_packs, &pack);
    }

    packet->payload = payload;
}
DEF_PKT_DECODER(cfg_keep_alive) {
    PacketKeepAlive* payload = arena_callocate(arena, sizeof *payload, ALLOC_TAG_PACKET);

    i64 tmp;
    bytebuf_read(bytes, sizeof payload->keep_alive_id, &tmp);
    payload->keep_alive_id = ntoh64(tmp);
    
    packet->payload = payload;
}
