#include "connection.h"
#include "decoders.h"
#include "encoders.h"
#include "handlers.h"
#include "logger.h"
#include "memory/arena.h"
#include "network/security.h"
#include "packet.h"

#include <platform/mc_mutex.h>
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
    const char* name;
} PacketFunction;

static PacketFunction function_table[_STATE_COUNT][_STATE_COUNT] = {
    [STATE_HANDSHAKE] =
        {
                           [PKT_HANDSHAKE] =
                {
                    &pkt_decode_handshake,
                    &pkt_handle_handshake,
                    &pkt_encode_dummy,
                    "HANDSHAKE",
                }, },
    [STATE_STATUS] =
        {
                           [PKT_STATUS] =
                {
                    &pkt_decode_dummy,
                    &pkt_handle_status,
                    &pkt_encode_status,
                    "STATUS",
                }, [PKT_PING] =
                {
                    &pkt_decode_ping,
                    &pkt_handle_ping,
                    &pkt_encode_ping,
                    "PING",
                }, },
    [STATE_LOGIN] =
        {
                           [PKT_LOG_START] =
                {
                    &pkt_decode_log_start,
                    &pkt_handle_log_start,
                    &pkt_encode_dummy,
                    "LOG_START",
                }, [PKT_ENC_REQ] =
                {
                    &pkt_decode_enc_res,
                    &pkt_handle_enc_res,
                    &pkt_encode_enc_req,
                    "ENC_REQ",
                }, [PKT_LOG_SUCCESS] =
                {
                    &pkt_decode_dummy,
                    &pkt_handle_dummy,
                    &pkt_encode_log_success,
                    "LOG_SUCCESS",
                }, [PKT_COMPRESS] =
                {
                    &pkt_decode_dummy,
                    &pkt_handle_dummy,
                    &pkt_encode_compress,
                    "COMPRESS",
                }, },
};

static PacketFunction* get_pkt_funcs(const Packet* pkt, const Connection* conn) {
    enum PacketType type = pkt->id;
    enum State state = conn->state;

    PacketFunction* row = function_table[state];
    if (!row) {
        log_errorf("Could not get packet functions: connection is in an invalid state %i.", state);
        return NULL;
    }
    PacketFunction* funcs = &row[pkt->id];
    if (!funcs->decoder) {
        log_errorf("Could not get packet functions: state = %i, type = %i.", state, type);
        return NULL;
    }
    return funcs;
}

pkt_acceptor get_pkt_handler(const Packet* pkt, Connection* conn) {
    PacketFunction* funcs = get_pkt_funcs(pkt, conn);
    if (!funcs)
        return NULL;
    return funcs->handler;
}

pkt_decoder get_pkt_decoder(const Packet* pkt, Connection* conn) {
    PacketFunction* funcs = get_pkt_funcs(pkt, conn);
    if (!funcs)
        return NULL;
    return funcs->decoder;
}

pkt_encoder get_pkt_encoder(const Packet* pkt, Connection* conn) {
    PacketFunction* funcs = get_pkt_funcs(pkt, conn);
    if (!funcs)
        return NULL;
    return funcs->encoder;
}

const char* get_pkt_name(const Packet* pkt, const Connection* conn) {
    PacketFunction* funcs = get_pkt_funcs(pkt, conn);
    if (!funcs)
        return NULL;
    return funcs->name;
}

bool conn_is_resuming_read(const Connection* conn) {
    return conn->recv_buffer.capacity > 0;
}

Connection
conn_create(int sockfd, u64 table_index, EncryptionContext* enc_ctx, string addr, u32 port) {
    Connection conn = {
        .persistent_arena = arena_create(CONN_PARENA_SIZE),
        .scratch_arena = arena_create(CONN_SARENA_SIZE),
        .compression = FALSE,
        .encryption = FALSE,
        .state = STATE_HANDSHAKE,
        .global_enc_ctx = enc_ctx,
        .sockfd = sockfd,
        .has_read_size = FALSE,
        .recv_buffer = {0},
        .can_send = TRUE,
        .send_buffer = bytebuf_create_fixed(CONN_BYTEBUF_SIZE, &conn.persistent_arena),
        .table_index = table_index,
        .peer_addr = str_create_copy(&addr, &conn.persistent_arena),
        .peer_port = port,
    };
    mcmutex_create(&conn.mutex);

    return conn;
}

bool conn_is_closed(const Connection* conn) {
    return conn->sockfd != -1;
}
