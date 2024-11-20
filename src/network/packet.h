/**
 *
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
 * @ingroup networking
 * @defgroup packets
 * These functions are used to get packet functions that depend on the type of packets.
 * @ingroup networking
 * @addtogroup packets
 *
 * @{
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
 * Enumeration of packet types.
 *
 * This enumeration enumeration defines packet identifiers.
 * The following table indicates in which phase the packet types are sent/received.
 *
 * @remark Multiple enumeration constants have the same value, the packet type
 * being local to the connection's state.
 */
enum PacketType {
    PKT_INVALID = -1,
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
 * Base structure of a packet.
 *
 * All packets share this structure.
 * The @ref pkt#payload field is only used internally. It is not sent to clients.
 */
typedef struct Packet {
    enum PacketType id; /**< The packet identifier. */
    u32 total_length;   /**< The length of the packet, including the ID and the payload. */
    u32 data_length;    /**< The length of the payload and packet ID (as a VarInt). Only used to
                             resume packet decoding. */
    u32 payload_length; /**< The length of the payload. */
    void* payload;      /**< A pointer to the payload. */
} Packet;

/**
 * First packet sent when a client connects to a server.
 *
 * This packet holds the server's address and port the client is trying to connect to,
 * as well as the version number of the MC protocol to use.
 *
 * This packet is also used to switch the state of the connection, to either get the server ping
 * or to join the server and play.
 */
typedef struct {
    /** The MC protocol version. */
    int protocol_version;
    /** The client is trying to connect to this address. */
    string srv_addr;
    /** The client is trying to connect to this port. */
    u16 srv_port;
    /** The next state of the connection. */
    int next_state;
} PacketHandshake;

/**
 * The status of server sent after a ping request.
 *
 * There is no payload for status request packets, thus no structure is needed.
 */
typedef struct {
    /** Status of the server, as a serialized JSON object. */
    string data;
} PacketStatusResponse;

/**
 * Packet sent and received to calculate the latency between the client
 * and the server.
 *
 * This structure is used for client-bound *ping requests* **and** server-bound *pong responses*.
 */
typedef struct {
    /**
     * The time at which the ping request was sent. The pong response should simply
     * send the same number as received in the ping request.
     */
    long num;
} PacketPing;

/**
 * Packet received when starting the logging sequence.
 *
 * This packet contains the name and UUID of the connecting player.
 */
typedef struct {
    string player_name;
    u64 uuid[2];
} PacketLoginStart;

/**
 * Packet sent to enable encryption of packets.
 */
typedef struct {
    string server_id;      /**< Arbitrary ID of the server.
         Empty in official servers, so it is empty here too. */
    i32 pkey_length;       /**< Length (in bytes) of the public key. */
    u8* pkey;              /**< Public key of the server. */
    i32 verify_tok_length; /**< Length of the verification token. */
    u8* verify_tok;        /**< Arbitrary bytes used to verify the protocol encryption. */
    bool authenticate;     /**< @ref TRUE if the client should authenticate through MC servers,
                          @ref FALSE otherwise.*/
} PacketEncReq;

/**
 * Packet received when encryption is enabled.
 */
typedef struct {
    i32 shared_secret_length;
    u8* shared_secret; /**< Byte array encrypted using the server's public key.
                       Used for AES encryption/decryption later on. */
    i32 verify_token_length;
    u8* verify_token; /**< Token sent in the @ref PacketEncReq "encryption request",
                         encrypted using the server's public key. */
} PacketEncRes;

/**
 * Packet sent to enable compression of packets.
 *
 * Compression is done using ZLib.
 * @remark This packet is optional and not sent when compression is not enabled.
 */
typedef struct {
    /** Compression threshold.
     *
     * Packets of equal length or larger are sent compressed.
     * A negative threshold will disable compression.
     */
    i64 threshold;
} PacketSetCompress;

/**
 * 
 */
typedef struct {
    string name;
    string value;
    bool is_signed;
    string signature;
} PlayerProperty;

/**
 * Packet sent to conclude the login process.
 * 
 * The server sends the player name and UUID as given by Mojang's authentication server,
 * as well as other player properties (e.g. skin texture)
 */
typedef struct {
    u64 uuid[2];
    string username;
    Vector properties; /**< Vector of @ref PlayerProperty. */
    /** Added in 1.20.5 to help modded servers switch to the new datapack system. */
    bool strict_errors;
} PacketLoginSuccess;

/**
 * Packet handler function.
 *
 * Processing of packets is done by packet handlers. Most packet handlers
 * trigger events, or send other packets.
 * @param ctx Context information of the network sub-system.
 * @param[in] pkt The packet to handle.
 * @param[in] conn The connection the packet was received on.
 * @return @ref TRUE if the packet was handled successfully, @ref FALSE otherwise.
 */
typedef bool (*pkt_acceptor)(NetworkContext* ctx, const Packet* pkt, Connection* conn);
/**
 * Packet decoder function.
 *
 * Decoder functions are responsible for reading raw bytes received directly from the socket,
 * and initialize and populate the given @ref Packet structure.
 * Decoder funtions do the inverse operation of encoder functions.
 *
 * @param[out] pkt The packet to populate (put data in members).
 * @param arena The arena to make allocations with. Typically used to allocate the packet's payload.
 * @param[in] bytes The buffer containing the packet's raw bytes.
 */
typedef void (*pkt_decoder)(Packet* pkt, Arena* arena, ByteBuffer* bytes);
/**
 * Packet encoder function.
 *
 * Encoder functions are responsible for converting a @ref Packet structure to a stream of raw bytes.
 * Raw bytes are then sent over the network to clients.
 * Encoder funtions do the inverse operation of decoder functions.
 */
typedef void (*pkt_encoder)(const Packet* pkt, ByteBuffer* buffer);

/**
 * Infer a packet handler from the specified packet and connection.
 *
 * The packet handler is inferred from the packet type and the connection state.
 *
 * @param[in] pkt The packet.
 * @param[in] conn The connection.
 * @return The inferred packet handler, or NULL if no handler was registered
 * for this packet - connection combination. 
 */
pkt_acceptor get_pkt_handler(const Packet* pkt, Connection* conn);
/**
 * Infer a packet decoder from the specified packet and connection.
 *
 * The packet decoder is inferred from the packet type and the connection state.
 *
 * @param[in] pkt The packet.
 * @param[in] conn The connection.
 * @return The inferred packet decoder, or NULL if no decoder was registered
 * for this packet - connection combination. 
 */
pkt_decoder get_pkt_decoder(const Packet* pkt, Connection* conn);
/**
 * Infer a packet encoder from the specified packet and connection.
 *
 * The packet encoder is inferred from the packet type and the connection state.
 *
 * @param[in] pkt The packet.
 * @param[in] conn The connection.
 * @return The inferred packet encoder, or @ref NULL if no encoder was registered
 * for this packet - connection combination. 
 */
pkt_encoder get_pkt_encoder(const Packet* pkt, Connection* conn);
/**
 * Get a packet type's name.
 *
 * This functions gets the name of the corresponding @ref PacketType enumeration constant.
 *
 * Because several constants have the same value, the name has to be inferred by the packet's
 * type value AND the connection state.
 *
 * @param[in] pkt The packet.
 * @param[in] conn The connection.
 * @return The name of the packet's type, or NULL if no name was registered
 * for this packet - connection combination. 
 */
const char* get_pkt_name(const Packet* pkt, const Connection* conn);

#endif /* ! PACKET_H */

/** @} */
