#include "connection.h"
#include "../utils/bitwise.h"
#include "decoders.h"
#include "handlers.h"
#include "packet.h"

#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>

static pkt_acceptor handler_table[][_STATE_COUNT] = {
    [STATE_HANDSHAKE] = {[PKT_HANDSHAKE] = &pkt_handle_handshake},
    [STATE_STATUS] = {[PKT_STATUS] = &pkt_handle_status, [PKT_PING] = &pkt_handle_ping}};

static pkt_decoder decoder_table[][_STATE_COUNT] = {
    [STATE_HANDSHAKE] = {[PKT_HANDSHAKE] = &pkt_decode_handshake},
    [STATE_STATUS] = {[PKT_STATUS] = NULL, [PKT_PING] = &pkt_decode_ping}};

pkt_acceptor get_pkt_handler(Packet* pkt, Connection* conn) {
    return handler_table[conn->state][pkt->id];
}

pkt_decoder get_pkt_decoder(Packet* pkt, Connection* conn) {
    return decoder_table[conn->state][pkt->id];
}

void conn_reset_buffer(Connection* conn, void* new_buffer, size_t size) {
    conn->bytes_read = 0;
    conn->buffer_size = size;
    conn->pkt_buffer = new_buffer;
}

bool conn_is_resuming_read(const Connection* conn) {
    return conn->pkt_buffer != NULL;
}

Connection conn_create(int sockfd) {
    Connection conn = {.arena = arena_create(1 << 16),
                       .compression = FALSE,
                       .state = STATE_HANDSHAKE,
                       .sockfd = sockfd,
                       .size_read = FALSE,
                       .send_arena = arena_create(1 << 24)};
    conn_reset_buffer(&conn, NULL, 0);

    return conn;
}

enum IOCode conn_read_bytes(Connection* conn) {
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

enum IOCode conn_send_bytes(Connection* conn) {
    ssize_t sent;

    while (conn->send_arena.length > 0) {
        sent = send(conn->sockfd, conn->send_arena.block, conn->send_arena.length, 0);

        if (sent == -1)
            return errno == EAGAIN || errno == EWOULDBLOCK ? IOC_AGAIN : IOC_ERROR;

        void* new_begin = offset(conn->send_arena.block, sent);
        conn->send_arena.length -= sent;
        memmove(conn->send_arena.block, new_begin, conn->send_arena.length);
    }
    return IOC_OK;
}
