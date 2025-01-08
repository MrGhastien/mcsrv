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
            &PKT_DECODER(handshake),
            &PKT_HANDLER(handshake),
            &PKT_ENCODER(dummy),
            "HANDSHAKE",
            NULL,
        },
    },
    [STATE_STATUS] = {
        [PKT_STATUS] = {
            &PKT_DECODER(dummy),
            &PKT_HANDLER(status),
            &PKT_ENCODER(status),
            "STATUS_REQUEST",
            "STATUS_RESPONSE",
        },
        [PKT_STATUS_PING] = {
            &PKT_DECODER(ping),
            &PKT_HANDLER(ping),
            &PKT_ENCODER(ping),
            "PING",
            "PONG",
        },
    },
    [STATE_LOGIN] = {
        [PKT_LOGIN_DISCONNECT] = {
            &PKT_DECODER(login_start),
            &PKT_HANDLER(login_start),
            &PKT_ENCODER(dummy),
            "LOGIN_START",
            "DISCONNECT",
        },
        [PKT_LOGIN_CRYPT_REQUEST] = {
            &PKT_DECODER(crypt_response),
            &PKT_HANDLER(crypt_response),
            &PKT_ENCODER(crypt_request),
            "CRYPT_RESPONSE",
            "CRYPT_REQUEST",
        },
        [PKT_LOGIN_SUCCESS] = {
            NULL,
            NULL,
            &PKT_ENCODER(login_success),
            "LOGIN_CUSTOM_RESPONSE",
            "LOGIN_SUCCESS",
        },
        [PKT_LOGIN_COMPRESS] = {
            &PKT_DECODER(dummy),
            &PKT_HANDLER(login_ack),
            &PKT_ENCODER(compress),
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
    [STATE_CONFIG] = {
        [PKT_CFG_COOKIE_REQUEST] = {
            &PKT_DECODER(cfg_client_info),
            &PKT_HANDLER(cfg_client_info),
            NULL,
            "CLIENT_INFO",
            "COOKIE_REQUEST"
        },
        [PKT_CFG_CUSTOM_CLIENT] = {
            NULL,
            NULL,
            &PKT_ENCODER(cfg_custom),
            "COOKIE_RESPONSE",
            "CUSTOM_CLIENT"
        },
        [PKT_CFG_DISCONNECT] = {
            &PKT_DECODER(cfg_custom),
            &PKT_HANDLER(cfg_custom),
            &PKT_ENCODER(cfg_disconnect),
            "CUSTOM_SERVER",
            "DISCONNECT"
        },
        [PKT_CFG_FINISH] = {
            &PKT_DECODER(cfg_finish_config_ack),
            &PKT_HANDLER(cfg_finish_config_ack),
            &PKT_ENCODER(dummy),
            "FINISH_ACK",
            "FINISH"
        },
        [PKT_CFG_CLIENT_KEEP_ALIVE] = {
            &PKT_DECODER(cfg_keep_alive),
            &PKT_HANDLER(cfg_keep_alive),
            &PKT_ENCODER(cfg_keep_alive),
            "SERVER_KEEP_ALIVE",
            "CLIENT_KEEP_ALIVE"
        },
        [PKT_CFG_PING] = {
            &PKT_DECODER(cfg_pong),
            &PKT_HANDLER(cfg_pong),
            &PKT_ENCODER(cfg_ping),
            "PONG",
            "PING"
        },
        [PKT_CFG_RESET_CHAT] = {
            &PKT_DECODER(cfg_respack_response),
            &PKT_HANDLER(cfg_respack_response),
            &PKT_ENCODER(dummy),
            "RESPACK_RESPONSE",
            "RESET_CHAT"
        },
        [PKT_CFG_REGISTRY_DATA] = {
            &PKT_DECODER(cfg_known_datapacks),
            &PKT_HANDLER(cfg_known_datapacks),
            &PKT_ENCODER(cfg_registry_data),
            "KNOWN_DATAPACKS_SERVER",
            "REGISTRY_DATA"
        },
        [PKT_CFG_REMOVE_RESPACK] = {
            NULL,
            NULL,
            &PKT_ENCODER(cfg_remove_respack),
            NULL,
            "REMOVE_RESPACK"
        },
        [PKT_CFG_ADD_RESPACK] = {
            NULL,
            NULL,
            &PKT_ENCODER(cfg_add_respack),
            NULL,
            "ADD_RESPACK"
        },
        [PKT_CFG_STORE_COOKIE] = {
            NULL,
            NULL,
            NULL,
            NULL,
            "STORE_COOKIE"
        },
        [PKT_CFG_TRANSFER] = {
            NULL,
            NULL,
            &PKT_ENCODER(cfg_transfer),
            NULL,
            "TRANSFER"
        },
        [PKT_CFG_SET_FEATURE_FLAGS] = {
            NULL,
            NULL,
            &PKT_ENCODER(cfg_set_feature_flags),
            NULL,
            "SET_FEATURE_FLAGS"
        },
        [PKT_CFG_UPDATE_TAGS] = {
            NULL,
            NULL,
            &PKT_ENCODER(cfg_update_tags),
            NULL,
            "UPDATE_TAGS"
        },
        [PKT_CFG_KNOWN_DATAPACKS_CLIENT] = {
            NULL,
            NULL,
            &PKT_ENCODER(cfg_known_datapacks),
            NULL,
            "KNOWN_DATAPACKS_CLIENT"
        },
        [PKT_CFG_CUSTOM_REPORT] = {
            NULL,
            NULL,
            &PKT_ENCODER(cfg_custom_report),
            NULL,
            "CUSTOM_REPORT"
        },
        [PKT_CFG_SERVER_LINKS] = {
            NULL,
            NULL,
            &PKT_ENCODER(cfg_server_links),
            NULL,
            "SERVER_LINKS"
        },
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
