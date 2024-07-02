#ifndef PACKET_H
#define PACKET_H

#include "../definitions.h"
#include "containers/bytebuffer.h"
#include "connection.h"
#include "../memory/arena.h"
#include "../utils/string.h"
#include <stddef.h>

enum PacketType {
    // === HANDSHAKE ===
    PKT_HANDSHAKE = 0x0,

    // === STATUS ===
    PKT_STATUS = 0x0,
    PKT_PING = 0x1,

    // === LOGIN ===
    // Client-bound
    PKT_DISCONNECT = 0x0,
    PKT_ENC_REQ = 0x1,
    PKT_LOG_SUCCESS = 0x2,
    PKT_COMPRESS = 0x3,
    PKT_LOG_PLUGIN_REQ = 0x4,
    PKT_COOKIE_REQ = 0x5,
    // Server-bound
    PKT_LOG_START = 0x0,
    PKT_ENC_RES = 0x1,
    PKG_LOG_PLUGIN_RES = 0x2,
    PKT_LOG_ACK = 0x3,
    PKT_COOKIE_RES = 0x4,
};

typedef struct raw_pkt {
    void* data;
    size_t size;
} RawPacket;

typedef struct pkt {
    enum PacketType id;
    size_t total_length;
    size_t payload_length;
    void* payload;
} Packet;

typedef struct {
    int protocol_version;
    string srv_addr;
    u16 srv_port;
    int next_state;
} PacketHandshake;

typedef struct {
    string data;
} PacketStatusResponse;

typedef struct {
    long num;
} PacketPing;

typedef struct {
    string player_name;
    u64 uuid[2];
} PacketLoginStart;

typedef struct {
    string server_id;
    i32 pkey_length;
    u8* pkey;
    i32 verify_tok_length;
    u8* verify_tok;
    bool authenticate;
} PacketEncReq;

typedef struct {
    i32 shared_secret_length;
    u8* shared_secret;
    i32 verify_token_length;
    u8* verify_token;
} PacketEncRes;

typedef struct {
    i64 threadhold;
} PacketSetCompress;

typedef bool (*pkt_acceptor)(const Packet* pkt, Connection* conn);
typedef void (*pkt_decoder)(Packet* pkt, Arena* arena, u8* raw);
typedef void (*pkt_encoder)(const Packet* pkt, ByteBuffer* buffer);

pkt_acceptor get_pkt_handler(const Packet* pkt, Connection* conn);
pkt_decoder get_pkt_decoder(const Packet* pkt, Connection* conn);
pkt_encoder get_pkt_encoder(const Packet* pkt, Connection* conn);

#endif /* ! PACKET_H */
