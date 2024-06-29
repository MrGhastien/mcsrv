#include "connection.h"
#include "decoders.h"
#include "encoders.h"
#include "handlers.h"
#include "logger.h"
#include "packet.h"

#include <pthread.h>
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
    [STATE_HANDSHAKE] = {
        [PKT_HANDSHAKE] = {
            &pkt_decode_handshake,
            &pkt_handle_handshake,
            &pkt_encode_dummy,
        },
    },
    [STATE_STATUS] = {
        [PKT_STATUS] = {
            &pkt_decode_dummy,
            &pkt_handle_status,
            &pkt_encode_status,
        },
        [PKT_PING] = {
            &pkt_decode_ping,
            &pkt_handle_ping,
            &pkt_encode_ping,
        },
    },
    [STATE_LOGIN] = {
        [PKT_LOG_START] = {
            &pkt_decode_log_start,
            &pkt_handle_log_start,
            &pkt_encode_dummy,
        }
    }
};

static PacketFunction* get_pkt_funcs(const Packet* pkt, Connection* conn) {
    enum PacketType type = pkt->id;
    enum State state = conn->state;

    PacketFunction* row = function_table[state];
    if(!row) {
        log_errorf("Could not get packet functions: connection is in an invalid state %i.", state);
        return NULL;
    }
    PacketFunction* funcs = &row[pkt->id];
    if(!funcs->decoder) {
        log_errorf("Could not get packet functions: state = %i, type = %i.", state, type);
        return NULL;
    }
    return funcs;
}

pkt_acceptor get_pkt_handler(const Packet* pkt, Connection* conn) {
    PacketFunction* funcs = get_pkt_funcs(pkt, conn);
    if(!funcs)
        return NULL;
    return funcs->handler;
}

pkt_decoder get_pkt_decoder(const Packet* pkt, Connection* conn) {
    PacketFunction* funcs = get_pkt_funcs(pkt, conn);
    if(!funcs)
        return NULL;
    return funcs->decoder;
}

pkt_encoder get_pkt_encoder(const Packet* pkt, Connection* conn) {
    PacketFunction* funcs = get_pkt_funcs(pkt, conn);
    if(!funcs)
        return NULL;
    return funcs->encoder;
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
        .send_buffer = bytebuf_create_fixed(CONN_BYTEBUF_SIZE, &conn.arena),
        .table_index = table_index,
    };
    pthread_mutex_init(&conn.mutex, NULL);
    conn_reset_buffer(&conn, NULL, 0);

    return conn;
}
