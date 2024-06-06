#include "packet.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

static size_t read_varint(const Connection* conn, int* out) {
    int res = 0;
    int status;
    size_t position = 0;
    size_t size = 0;
    u8 byte = CONTINUE_BIT;
    while (TRUE) {
        status = recv(conn->sockfd, &byte, sizeof byte, 0);

        res |= (byte & SEGMENT_BITS) << position;

        position += 7;
        size++;

        if ((byte & CONTINUE_BIT) == 0)
            break;

        if (position >= 32)
            return FAIL;
    }

    if (status == -1) {
        perror("Failed to read varint");
        return -1;
    }

    *out = res;
    return size;
}

Packet* packet_read(Connection* conn) {
    int length;
    int id;
    if (read_varint(conn, &length) == FAIL)
        return NULL;

    u8* buf = arena_allocate(&conn->arena, length);
    if (!buf)
        return NULL;

    socket_readbytes(conn->sockfd, buf, length);

    size_t id_size = decode_varint(buf, &id);
    if (id_size == FAIL)
        return NULL;

    Packet pkt;

    pkt.total_length = length;
    pkt.payload_length = length - id_size;
    pkt.id = id;

    pkt_decoder decoder = get_pkt_decoder(&pkt, conn);
    if(decoder)
        decoder(&pkt, conn, buf + id_size);


    puts("====");
    printf("Received packet:\nSize: %zu\nID: %i\n", pkt.total_length, pkt.id);

    putchar('[');
    for (size_t i = 0; i < pkt.payload_length; i++) {
        printf("%02x ", buf[i + id_size]);
    }
    putchar(']');
    putchar('\n');

    arena_free(&conn->arena, length);

    Packet* out = arena_allocate(&conn->arena, sizeof *out);
    if (!out)
        return NULL;

    

    return out;
}

void packet_write(const Packet* pkt, Connection* conn, pkt_encoder encoder) {
    Arena* arena = &conn->arena;

    encoder(pkt, conn);

    send(conn->sockfd, arena->block, arena->length, 0);

}
