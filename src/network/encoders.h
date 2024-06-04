#ifndef ENCODERS_H
#define ENCODERS_H

#include "packet.h"
#include "utils.h"
#include <string.h>

static void encode_varint(int n, Arena* arena) {
    while (TRUE) {
        u8* byte = arena_allocate(arena, sizeof *byte);
        *byte = n & SEGMENT_BITS;
        if (n & ~SEGMENT_BITS)
            *byte |= CONTINUE_BIT;

        n >>= 7;
    }
}

static void encode_string(char* str, Arena* arena) {
    size_t len = strlen(str);
    encode_varint(len, arena);

    char c;
    while((c = *str)) {
        char* chr = arena_allocate(arena, sizeof *chr);
        *chr = c;
    }
}

void pkt_encode_status(const Packet* pkt, const Connection* conn, Arena* arena) {
    //TODO: JSON parser / serializer !
}

#endif /* ! ENCODERS_H */
