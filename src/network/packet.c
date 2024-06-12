#include "packet.h"
#include "connection.h"
#include "utils.h"
#include <errno.h>
#include <stdio.h>
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

    if (!conn_is_resuming_read(conn)) {
        // New packet
        void* buf = arena_allocate(&conn->arena, 4);
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
        arena_free(&conn->arena, 4);
        conn->size_read = TRUE;

        u8* buf = arena_allocate(&conn->arena, length);
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

enum IOCode packet_write(const Packet* pkt, Connection* conn, pkt_encoder encoder) {
    Arena buffer_arena = arena_create(PKT_MAX_SIZE);
    if (!conn_is_resuming_read(conn)) {
        encode_varint_arena(pkt->id, &buffer_arena);
        encoder(pkt, conn, &buffer_arena);
    }

    
    u8 len_buf[4];
    ssize_t sent = encode_varint(buffer_arena.length, len_buf);

    puts("Sending:");
    putchar('[');
    for (ssize_t i = 0; i < sent; i++) {
        u8 byte = len_buf[i];
        printf("%02x ", byte);
    }
    for (size_t i = 0; i < buffer_arena.length; i++) {
        u8 byte = ((u8*)buffer_arena.block)[i];
        printf("%02x ", byte);
    }
    putchar(']');
    putchar('\n');

    sent = send(conn->sockfd, len_buf, sent, 0);
    sent = send(conn->sockfd, buffer_arena.block, buffer_arena.length, 0);
    if (sent == -1)
        return errno == EWOULDBLOCK || errno == EAGAIN ? IOC_AGAIN : IOC_ERROR;

    arena_destroy(&buffer_arena);
    return IOC_OK;
}
