/**
 * @file packet.h
 * @author Bastien Morino
 *
 * @brief Packet types definitions.
 *
 * The following header defines packet types for the Minecraft Protocol.
 * Packets are comprised of :
 * - A @ref PacketType "packet identifier", or packet type, represented by an unsigned number,
 * - The total length of the packet,
 * - The length of the packet's payload,
 * - The actual payload.
 *
 * The payload depends on the packet ID.
 *
 * @see https://wiki.vg/Protocol
 */

#ifndef PACKET_H
#define PACKET_H

#include "connection.h"
#include "containers/bytebuffer.h"
#include "containers/vector.h"
#include "definitions.h"
#include "memory/arena.h"
#include "utils/string.h"

#include <stddef.h>

/**
 * @brief Enumeration of packet types.
 *
 * This enumeration enumeration defines packet identifiers.
 * The following table indicates in which phase the packet types are sent/received.
 *
 * @remark Multiple enumeration constants have the same value, the packet type
 * being local to the connection's state.
 */
enum PacketType {
    // === HANDSHAKE ===
    /** @name Handshake */
    /**@{*/
    /** @ref State#STATE_HANDSHAKE "Hand-shake" phase */
    PKT_HANDSHAKE = 0x0,
    /**@}*/

    // === STATUS ===
    /** @name Status */
    /**@{*/
    /** @ref State#STATE_STATUS "Status" phase (Server-Bound) */
    PKT_STATUS = 0x0,
    /** @ref State#STATE_STATUS "Status" phase (Client-Bound) */
    PKT_PING = 0x1,
    /**@}*/

    // === LOGIN ===
    /** @name Log-in */
    /**@{*/
    // Client-bound
    /** @ref State#STATE_LOGIN "Log-in" phase (Client-bound) */
    PKT_DISCONNECT = 0x0,
    PKT_ENC_REQ = 0x1,
    PKT_LOG_SUCCESS = 0x2,
    PKT_COMPRESS = 0x3,
    PKT_LOG_PLUGIN_REQ = 0x4,
    PKT_COOKIE_REQ = 0x5,
    /**@}*/
    // Server-bound
    /**@{*/
    /** @ref State#STATE_LOGIN "Log-in" phase (Server-bound) */
    PKT_LOG_START = 0x0,
    PKT_ENC_RES = 0x1,
    PKG_LOG_PLUGIN_RES = 0x2,
    PKT_LOG_ACK = 0x3,
    PKT_COOKIE_RES = 0x4,
    /**@}*/
};

/**
 * @brief Base structure of a packet.
 *
 * All packets share this structure.
 * The @ref pkt#payload field is only used internally. It is not sent to clients.
 */
typedef struct pkt {
    enum PacketType id; /**< The packet identifier. */
    u64 total_length; /**< The length of the packet, including the ID and the payload. */
    u64 payload_length; /**< The length of the payload. */
    void* payload; /**< A pointer to the payload. */
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

typedef struct {
    string name;
    string value;
    bool is_signed;
    string signature;
} PlayerProperty;

typedef struct {
    u64 uuid[2];
    string username;
    Vector properties;
    bool strict_errors;
} PacketLoginSuccess;

typedef bool (*pkt_acceptor)(const Packet* pkt, Connection* conn);
typedef void (*pkt_decoder)(Packet* pkt, Arena* arena, ByteBuffer* bytes);
typedef void (*pkt_encoder)(const Packet* pkt, ByteBuffer* buffer);

pkt_acceptor get_pkt_handler(const Packet* pkt, Connection* conn);
pkt_decoder get_pkt_decoder(const Packet* pkt, Connection* conn);
pkt_encoder get_pkt_encoder(const Packet* pkt, Connection* conn);
const char* get_pkt_name(const Packet* pkt, const Connection* conn);

#endif /* ! PACKET_H */
