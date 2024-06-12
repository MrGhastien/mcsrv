#include "encoders.h"
#include "../json/json.h"
#include "packet.h"
#include "utils.h"
#include <string.h>
#include <stdio.h>

static void encode_string(const string* str, Arena* arena) {
    encode_varint_arena(str->length, arena);

    void* ptr = arena_allocate(arena, str->length);
    memcpy(ptr, str->base, str->length);
}

void pkt_encode_status(const Packet* pkt, Connection* conn, Arena* arena) {
    (void)conn;
    PacketStatusResponse* payload = pkt->payload;
    encode_string(&payload->data, arena);
}

void pkt_encode_ping(const Packet *pkt, Connection *conn, Arena *arena) {
    (void)conn;
    PacketPing* pong = pkt->payload;

    u64* ptr = arena_allocate(arena, sizeof *ptr);
    *ptr = pong->num;
}
