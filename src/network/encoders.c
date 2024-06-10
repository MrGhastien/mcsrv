#include "encoders.h"
#include "../json/json.h"
#include "packet.h"
#include "utils.h"
#include <string.h>
#include <stdio.h>

static void encode_varint(int n, Arena* arena) {
    while (TRUE) {
        u8* byte = arena_allocate(arena, sizeof *byte);
        *byte = n & SEGMENT_BITS;
        if (n & ~SEGMENT_BITS)
            *byte |= CONTINUE_BIT;
        else
            return;

        n >>= 7;
    }
}

static void encode_string(const string* str, Arena* arena) {
    encode_varint(str->length, arena);

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
