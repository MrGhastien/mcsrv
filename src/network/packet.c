#include "packet.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#define PKT_MAX_SIZE 2097151UL

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

    Arena buf_arena = arena_create(length);
    u8* buf = arena_allocate(&buf_arena, length);
    if (!buf)
        return NULL;

    if(socket_readbytes(conn->sockfd, buf, length) <= 0)
        return NULL;

    size_t id_size = decode_varint(buf, &id);
    if (id_size == FAIL)
        return NULL;

    Packet* pkt = arena_allocate(&conn->arena, sizeof *pkt);
    if (!pkt)
        return NULL;

    pkt->total_length = length;
    pkt->payload_length = length - id_size;
    pkt->id = id;

    pkt_decoder decoder = get_pkt_decoder(pkt, conn);
    if(decoder)
        decoder(pkt, conn, buf + id_size);


    puts("====");
    printf("Received packet:\nSize: %zu\nID: %i\n", pkt->total_length, pkt->id);

    putchar('[');
    for (size_t i = 0; i < pkt->payload_length; i++) {
        printf("%02x ", buf[i + id_size]);
    }
    putchar(']');
    putchar('\n');

    arena_destroy(&buf_arena);

    return pkt;
}

void packet_write(const Packet* pkt, Connection* conn, pkt_encoder encoder) {
    Arena buffer_arena = arena_create(PKT_MAX_SIZE);

    encoder(pkt, conn, &buffer_arena);

    send(conn->sockfd, buffer_arena.block, buffer_arena.length, 0);

    arena_destroy(&buffer_arena);
}
