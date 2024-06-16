#include "connection.h"
#include "bytebuffer.h"
#include "decoders.h"
#include "encoders.h"
#include "handlers.h"
#include "packet.h"

#include <string.h>
#include <sys/socket.h>

#define CONN_ARENA_SIZE 33554432
#define CONN_BYTEBUF_SIZE 4194304

typedef struct pkt_func {
    pkt_decoder decoder;
    pkt_acceptor handler;
    pkt_encoder encoder;
} PacketFunction;

static PacketFunction function_table[_STATE_COUNT][_STATE_COUNT] = {
    [STATE_HANDSHAKE] =
        {
            [PKT_HANDSHAKE] = {&pkt_decode_handshake, &pkt_handle_handshake, NULL},
        },
    [STATE_STATUS] = {[PKT_STATUS] = {NULL, &pkt_handle_status, &pkt_encode_status},
                      [PKT_PING] = {&pkt_decode_ping, &pkt_handle_ping, &pkt_encode_ping}},
};

pkt_acceptor get_pkt_handler(const Packet* pkt, Connection* conn) {
    return function_table[conn->state][pkt->id].handler;
}

pkt_decoder get_pkt_decoder(const Packet* pkt, Connection* conn) {
    return function_table[conn->state][pkt->id].decoder;
}

pkt_encoder get_pkt_encoder(const Packet* pkt, Connection* conn) {
    return function_table[conn->state][pkt->id].encoder;
}

void conn_reset_buffer(Connection* conn, void* new_buffer, size_t size) {
    conn->bytes_read = 0;
    conn->buffer_size = size;
    conn->pkt_buffer = new_buffer;
}

bool conn_is_resuming_read(const Connection* conn) {
    return conn->pkt_buffer != NULL;
}

Connection conn_create(int sockfd, u64 table_index) {
    Connection conn = {
        .arena = arena_create(CONN_ARENA_SIZE),
        .compression = FALSE,
        .state = STATE_HANDSHAKE,
        .sockfd = sockfd,
        .has_read_size = FALSE,
        .send_buffer = bytebuf_create(CONN_BYTEBUF_SIZE, &conn.arena),
        .table_index = table_index
    };
    conn_reset_buffer(&conn, NULL, 0);

    return conn;
}
