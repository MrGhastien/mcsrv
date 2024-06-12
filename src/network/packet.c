#include "packet.h"
#include "connection.h"
#include "utils.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#define PKT_MAX_SIZE 2097151UL

static enum IOCode read_varint(Connection* conn) {
    u8 byte;

    while (TRUE) {
        ssize_t status = recv(conn->sockfd, &byte, sizeof byte, 0);
        if (status == -1)
            return errno == EWOULDBLOCK || errno == EAGAIN ? IOC_AGAIN : IOC_ERROR;

        conn->pkt_buffer[conn->bytes_read] = byte;
        conn->bytes_read++;

        if ((byte & CONTINUE_BIT) == 0)
            break;

        if (conn->bytes_read == 4)
            return IOC_ERROR;
    }

    return IOC_OK;
}

bool packet_decode(Connection* conn, Packet* out_pkt) {

    u8* buf = conn->pkt_buffer;
    out_pkt->total_length = conn->buffer_size;
    int id;
    size_t id_size = decode_varint(buf, &id);
    if (id_size == FAIL) {
        perror("Invalid id");
        return FALSE;
    }

    out_pkt->payload_length = out_pkt->total_length - id_size;
    out_pkt->id = id;

    pkt_decoder decoder = get_pkt_decoder(out_pkt, conn);
    if (decoder)
        decoder(out_pkt, conn, buf + id_size);

    puts("====");
    printf("Received packet:\nSize: %zu\nID: %i\n", out_pkt->total_length, out_pkt->id);

    /* putchar('['); */
    /* for (size_t i = 0; i < out_pkt->payload_length; i++) { */
    /*     printf("%02x ", buf[i + id_size]); */
    /* } */
    /* putchar(']'); */
    /* putchar('\n'); */

    return TRUE;
}

enum IOCode packet_read(Connection* conn) {
    Arena arena = conn->arena;

    if (!conn_is_resuming_read(conn)) {
        // New packet
        void* buf = arena_allocate(&arena, 4);
        conn_reset_buffer(conn, buf, 4);
        conn->size_read = FALSE;
    }

    enum IOCode code;
    int length;

    if (!conn->size_read) {
        code = read_varint(conn);
        if (code != IOC_OK)
            return code;

        decode_varint(conn->pkt_buffer, &length);
        conn->size_read = TRUE;

        u8* buf = arena_allocate(&arena, length);
        if (!buf) {
            perror("Could not allocate memory for the packet paylaod buffer");
            return IOC_ERROR;
        }

        conn_reset_buffer(conn, buf, length);
    }

    code = conn_read_bytes(conn);
    if (code != IOC_OK)
        return code;

    return IOC_OK;
}

void packet_write(const Packet* pkt, Connection* conn, pkt_encoder encoder) {
    Arena arena = conn->arena;
    arena_save(&arena);

    encode_varint_arena(pkt->id, &arena);
    encoder(pkt, conn, &arena);

    size_t length = arena_recent_length(&arena);
    encode_varint_arena(length, &conn->send_arena);
    void* data_ptr = arena_allocate(&conn->send_arena, length);

    memcpy(data_ptr, arena_recent_pos(&arena), length);

#ifdef DEBUG
        puts("Sending:");
    putchar('[');
    for (size_t i = 0; i < conn->send_arena.length; i++) {
        u8 byte = ((u8*)conn->send_arena.block)[i];
        printf("%02x ", byte);
    }
    putchar(']');
    putchar('\n');
#endif
}
