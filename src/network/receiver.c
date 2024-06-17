#include "receiver.h"
#include "utils/bitwise.h"
#include "connection.h"
#include "network.h"
#include "packet.h"
#include "utils.h"

#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>

static enum IOCode read_varint(Connection* conn) {
    u8 byte;

    while (TRUE) {
        ssize_t status = recv(conn->sockfd, &byte, sizeof byte, 0);
        if (status == -1)
            return errno == EWOULDBLOCK || errno == EAGAIN ? IOC_AGAIN : IOC_ERROR;
        else if(status == 0)
            return IOC_CLOSED;

        conn->pkt_buffer[conn->bytes_read] = byte;
        conn->bytes_read++;

        if ((byte & CONTINUE_BIT) == 0)
            break;

        if (conn->bytes_read == 4)
            return IOC_ERROR;
    }

    return IOC_OK;
}

static enum IOCode read_bytes(Connection* conn) {
    size_t total_read = 0;
    while (conn->bytes_read < conn->buffer_size) {
        void* ptr = offset(conn->pkt_buffer, conn->bytes_read);
        size_t remaining = conn->buffer_size - conn->bytes_read;
        ssize_t immediate_read = recv(conn->sockfd, ptr, remaining, 0);
        if (immediate_read < 0)
            return errno == EAGAIN || errno == EWOULDBLOCK ? IOC_AGAIN : IOC_ERROR;

        conn->bytes_read += immediate_read;
    }
    return total_read;
}

enum IOCode read_packet(Connection* conn, Arena* arena) {

    if (!conn_is_resuming_read(conn)) {
        // New packet
        int* buf = arena_allocate(arena, sizeof *buf);
        conn_reset_buffer(conn, buf, sizeof *buf);
        conn->has_read_size = FALSE;
    }

    enum IOCode code;
    int length;

    if (!conn->has_read_size) {
        code = read_varint(conn);
        if (code != IOC_OK)
            return code;

        decode_varint(conn->pkt_buffer, &length);
        arena_free(arena, sizeof(int));
        conn->has_read_size = TRUE;

        u8* buf = arena_allocate(arena, length);

        conn_reset_buffer(conn, buf, length);
    }

    return read_bytes(conn);
}

static bool decode_packet(RawPacket* raw, Connection* conn, Packet* out_pkt) {
    out_pkt->total_length = raw->size;

    int id;
    size_t id_size = decode_varint(raw->data, &id);
    if (id_size == FAIL) {
        perror("Invalid id");
        return FALSE;
    }

    out_pkt->payload_length = out_pkt->total_length - id_size;
    out_pkt->id = id;

    pkt_decoder decoder = get_pkt_decoder(out_pkt, conn);
    if (decoder)
        decoder(out_pkt, &conn->arena, offset(raw->data, id_size));

    puts("====");
    printf("Received packet:\n  - Size: %zu\n  - ID: %i\n", out_pkt->total_length, out_pkt->id);

    return TRUE;
}

static void handle_packet(const Packet* pkt, Connection* conn) {
    pkt_acceptor handler = get_pkt_handler(pkt, conn);
    if (handler)
        handler(pkt, conn);
}

enum IOCode receive_packet(Connection* conn) {

    enum IOCode code = IOC_OK;

    while (code == IOC_OK) {
        if(!conn_is_resuming_read(conn))
            arena_save(&conn->arena);
        code = read_packet(conn, &conn->arena);
        if (code != IOC_OK)
            return code;

        RawPacket raw = {.data = conn->pkt_buffer, .size = conn->buffer_size};
        Packet pkt;

        decode_packet(&raw, conn, &pkt);
        conn_reset_buffer(conn, NULL, 0);

        handle_packet(&pkt, conn);

        arena_restore(&conn->arena);
    }

    return code;
}