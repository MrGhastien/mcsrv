#include "connection.h"
#include "decoders.h"
#include "encoders.h"
#include "handlers.h"
#include "network/compression.h"
#include "packet.h"
#include "packet_codec.h"
#include "security.h"

#include "logger.h"
#include "memory/arena.h"

#include "platform/mc_mutex.h"
#include "platform/socket.h"

#define CONN_PARENA_SIZE 33554432
#define CONN_SARENA_SIZE 4194304
#define CONN_BYTEBUF_SIZE 4194304

typedef struct pkt_func {
    pkt_decoder decoder;
    pkt_acceptor handler;
    pkt_encoder encoder;
    const char* serverbound_name;
    const char* clientbound_name;
} PacketFunction;

static PacketFunction function_table[_STATE_COUNT][_PKT_TYPE_COUNT] = {
    [STATE_HANDSHAKE] = {
        [PKT_HANDSHAKE] = {
            &pkt_decode_handshake,
            &pkt_handle_handshake,
            &pkt_encode_dummy,
            "HANDSHAKE",
            NULL,
        },
    },
    [STATE_STATUS] = {
        [PKT_STATUS] = {
            &pkt_decode_dummy,
            &pkt_handle_status,
            &pkt_encode_status,
            "STATUS_REQUEST",
            "STATUS_RESPONSE",
        },
        [PKT_STATUS_PING] = {
            &pkt_decode_ping,
            &pkt_handle_ping,
            &pkt_encode_ping,
            "PING",
            "PONG",
        },
    },
    [STATE_LOGIN] = {
        [PKT_LOGIN_DISCONNECT] = {
            &pkt_decode_log_start,
            &pkt_handle_log_start,
            &pkt_encode_dummy,
            "LOGIN_START",
            "DISCONNECT",
        },
        [PKT_LOGIN_CRYPT_REQUEST] = {
            &pkt_decode_enc_res,
            &pkt_handle_enc_res,
            &pkt_encode_enc_req,
            "CRYPT_RESPONSE",
            "CRYPT_REQUEST",
        },
        [PKT_LOGIN_SUCCESS] = {
            NULL,
            NULL,
            &pkt_encode_log_success,
            "LOGIN_CUSTOM_RESPONSE",
            "LOGIN_SUCCESS",
        },
        [PKT_LOGIN_COMPRESS] = {
            &pkt_decode_dummy,
            &pkt_handle_dummy,
            &pkt_encode_compress,
            "LOGIN_ACK",
            "COMPRESS",
        },
        [PKT_LOGIN_CUSTOM_REQUEST] = {
            NULL,
            NULL,
            NULL,
            "LOGIN_CUSTOM_REQUEST",
            "COOKIE_RESPONSE",
        },
        [PKT_LOGIN_COOKIE_REQUEST] = {
            NULL,
            NULL,
            NULL,
            NULL,
            "LOGIN_COOKIE_REQUEST",
        }
    },
};

static PacketFunction* get_pkt_funcs(const Packet* pkt, const Connection* conn) {
    enum PacketType type = pkt->id;
    enum State state = conn->state;

    PacketFunction* row = function_table[state];
    if (!row) {
        log_errorf("Could not get packet functions: connection is in an invalid state %i.", state);
        return NULL;
    }
    return &row[type];
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

const char* get_pkt_name(const Packet* pkt, const Connection* conn, bool clientbound) {
    PacketFunction* funcs = get_pkt_funcs(pkt, conn);
    if (!funcs)
        return NULL;
    return clientbound ? funcs->clientbound_name : funcs->serverbound_name;
}

bool conn_is_resuming_read(const Connection* conn) {
    return conn->packet_cache != NULL;
}

Connection
conn_create(socketfd sockfd, i64 table_index, EncryptionContext* enc_ctx, string addr, u32 port) {
    Connection conn = {
        .persistent_arena = arena_create(CONN_PARENA_SIZE, BLK_TAG_NETWORK),
        .scratch_arena = arena_create(CONN_SARENA_SIZE, BLK_TAG_NETWORK),
        .compression = FALSE,
        .encryption = FALSE,
        .state = STATE_HANDSHAKE,
        .global_enc_ctx = enc_ctx,
        .peer_socket = sockfd,
        .pending_send = FALSE,
        .pending_recv = FALSE,
        .recv_buffer = bytebuf_create_fixed(CONN_BYTEBUF_SIZE, &conn.persistent_arena),
        .send_buffer = bytebuf_create_fixed(CONN_BYTEBUF_SIZE, &conn.persistent_arena),
        .packet_cache = NULL,
        .table_index = table_index,
        .peer_addr = str_create_copy(&addr, &conn.persistent_arena),
        .peer_port = port,
    };
    mcmutex_create(&conn.mutex);

    return conn;
}

bool conn_is_closed(const Connection* conn) {
    return sock_is_valid(conn->peer_socket);
}
