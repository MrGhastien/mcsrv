#include "receiver.h"
#include "connection.h"
#include "containers/bytebuffer.h"
#include "logger.h"
#include "memory/arena.h"
#include "network.h"
#include "packet.h"
#include "utils/bitwise.h"

#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>

static enum IOCode read_varint(Connection* conn) {
    u8 byte;

    while (TRUE) {
        ssize_t status = recv(conn->sockfd, &byte, sizeof byte, 0);
        if (status == -1)
            return errno == EWOULDBLOCK || errno == EAGAIN ? IOC_AGAIN : IOC_ERROR;
        else if (status == 0)
            return IOC_CLOSED;

        if (conn->recv_buffer.size == 0 && byte == 0xfe) {
            log_error("Received legacy ping request. Not implemented yet!!");
            return IOC_ERROR;
        }

        bytebuf_write(&conn->recv_buffer, &byte, sizeof byte);

        if ((byte & CONTINUE_BIT) == 0)
            break;

        if (conn->recv_buffer.size == 4)
            return IOC_ERROR;
    }

    return IOC_OK;
}

static enum IOCode read_bytes(Connection* conn) {
    while (conn->recv_buffer.size < conn->recv_buffer.capacity) {
        void* ptr;
        u64 remaining = bytebuf_contiguous_write(&conn->recv_buffer, &ptr);
        while (remaining > 0) {
            ssize_t immediate_read = recv(conn->sockfd, ptr, remaining, 0);
            if (immediate_read < 0) {
                bytebuf_unwrite(&conn->recv_buffer, remaining);
                return errno == EAGAIN || errno == EWOULDBLOCK ? IOC_AGAIN : IOC_ERROR;
            }
            remaining -= immediate_read;
        }
    }
    return IOC_OK;
}

enum IOCode read_packet(Connection* conn, Arena* arena) {

    if (!conn_is_resuming_read(conn)) {
        // New packet
        conn->recv_buffer = bytebuf_create_fixed(sizeof(int), arena);
        conn->has_read_size = FALSE;
    }

    enum IOCode code;
    int length;

    if (!conn->has_read_size) {
        code = read_varint(conn);
        if (code != IOC_OK)
            return code;

        if(bytebuf_read_varint(&conn->recv_buffer, &length) <= 0)
            return IOC_ERROR;
        conn->has_read_size = TRUE;
        conn->recv_buffer = bytebuf_create_fixed(length, arena);
    }

    return read_bytes(conn);
}

static bool decode_packet(ByteBuffer* bytes, Connection* conn, Packet* out_pkt) {
    out_pkt->total_length = bytes->size;

    int id;
    i64 id_size = bytebuf_read_varint(bytes, &id);
    if (id_size <= 0) {
        log_error("Received invalid packet id");
        return FALSE;
    }

    out_pkt->payload_length = out_pkt->total_length - id_size;
    out_pkt->id = id;

    pkt_decoder decoder = get_pkt_decoder(out_pkt, conn);
    if (!decoder)
        return FALSE;
    decoder(out_pkt, &conn->scratch_arena, bytes);

    log_debugf("Packet IN: %s", get_pkt_name(out_pkt, conn));

    return TRUE;
}

static bool handle_packet(const Packet* pkt, Connection* conn) {
    pkt_acceptor handler = get_pkt_handler(pkt, conn);
    if (!handler)
        return FALSE;
    return handler(pkt, conn);
}

enum IOCode receive_packet(Connection* conn) {

    enum IOCode code = IOC_OK;

    //TODO: HANDLE DECOMPRESSION & DECRYPTION !

    while (code == IOC_OK) {
        if (!conn_is_resuming_read(conn))
            arena_save(&conn->scratch_arena);
        code = read_packet(conn, &conn->scratch_arena);
        if (code != IOC_OK)
            return code;

        Packet pkt;

        if (!decode_packet(&conn->recv_buffer, conn, &pkt))
            return IOC_ERROR;
        conn->has_read_size = FALSE;
        conn->recv_buffer = (ByteBuffer) {0};

        if (!handle_packet(&pkt, conn))
            return IOC_ERROR;

        arena_restore(&conn->scratch_arena);
    }

    return code;
}
