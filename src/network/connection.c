#include "connection.h"
#include "decoders.h"
#include "encoders.h"
#include "handlers.h"
#include "logger.h"
#include "memory/arena.h"
#include "network/encryption.h"
#include "packet.h"

#include <pthread.h>
#include <string.h>
#include <sys/socket.h>

#define CONN_PARENA_SIZE 33554432
#define CONN_SARENA_SIZE 4194304
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
        },
        [PKT_ENC_REQ] = {
            &pkt_decode_enc_res,
            &pkt_handle_enc_res,
            &pkt_encode_enc_req,
        },
        [PKT_COMPRESS] = {
            &pkt_decode_dummy,
            &pkt_handle_dummy,
            &pkt_encode_compress,
        },
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

void conn_reset_buffer(Connection* conn, void* new_buffer, u64 size) {
    conn->bytes_read = 0;
    conn->buffer_size = size;
    conn->pkt_buffer = new_buffer;
}

bool conn_is_resuming_read(const Connection* conn) {
    return conn->pkt_buffer != NULL;
}

Connection conn_create(int sockfd, u64 table_index, EncryptionContext* enc_ctx) {
    Connection conn = {
        .persistent_arena = arena_create(CONN_PARENA_SIZE),
        .scratch_arena = arena_create(CONN_SARENA_SIZE),
        .compression = FALSE,
        .encryption = FALSE,
        .state = STATE_HANDSHAKE,
        .global_enc_ctx = enc_ctx,
        .sockfd = sockfd,
        .has_read_size = FALSE,
        .send_buffer = bytebuf_create_fixed(CONN_BYTEBUF_SIZE, &conn.persistent_arena),
        .table_index = table_index,
    };
    pthread_mutex_init(&conn.mutex, NULL);
    conn_reset_buffer(&conn, NULL, 0);

    return conn;
}
