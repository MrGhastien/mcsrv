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

#include "containers/vector.h"
#include "data/json.h"
#include "data/nbt.h"
#include "definitions.h"
#include "resource/resource_id.h"
#include "utils/string.h"

#include <stddef.h>

/**
 * Enumeration of packet types.
 *
 * This enumeration defines packet identifiers.
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
    PKT_STATUS_PING = 0x1,
    /**@}*/

    // === LOGIN ===
    /** @name Log-in */
    /**@{*/
    // Client-bound
    /** @ref State#STATE_LOGIN "Log-in" phase (Client-bound) */
    PKT_LOGIN_DISCONNECT = 0x0,
    PKT_LOGIN_CRYPT_REQUEST = 0x1,
    PKT_LOGIN_SUCCESS = 0x2,
    PKT_LOGIN_COMPRESS = 0x3,
    PKT_LOGIN_CUSTOM_REQUEST = 0x4,
    PKT_LOGIN_COOKIE_REQUEST = 0x5,
    /**@}*/
    // Server-bound
    /**@{*/
    /** @ref State#STATE_LOGIN "Log-in" phase (Server-bound) */
    PKT_LOGIN_START = 0x0,
    PKT_LOGIN_CRYPT_RESPONSE = 0x1,
    PKT_LOGIN_CUSTOM_RESPONSE = 0x2,
    PKT_LOGIN_ACK = 0x3,
    PKT_LOGIN_COOKIE_RESPONSE = 0x4,
    /**@}*/

    // === CONFIGURATION ===
    /** @name Configuration */
    /**@{*/
    /** @ref State#STATE_CONFIG "Configuration" phase (Client-Bound) */
    PKT_CFG_COOKIE_REQUEST = 0x0,
    PKT_CFG_CUSTOM_CLIENT = 0x1,
    PKT_CFG_DISCONNECT = 0x2,
    PKT_CFG_FINISH = 0x3,
    PKT_CFG_CLIENT_KEEP_ALIVE = 0x4,
    PKT_CFG_PING = 0x5,
    PKT_CFG_RESET_CHAT = 0x6,
    PKT_CFG_REGISTRY_DATA = 0x7,
    PKT_CFG_REMOVE_RESPACK = 0x8,
    PKT_CFG_ADD_RESPACK = 0x9,
    PKT_CFG_STORE_COOKIE = 0xa,
    PKT_CFG_TRANSFER = 0xb,
    PKT_CFG_SET_FEATURE_FLAGS = 0xc,
    PKT_CFG_UPDATE_TAGS = 0xd,
    PKT_CFG_KNOWN_DATAPACKS_CLIENT = 0xe,
    PKT_CFG_CUSTOM_REPORT = 0xf,
    PKT_CFG_SERVER_LINKS = 0x10,
    /** @ref State#STATE_CONFIG "Configuration" phase (Server-Bound) */
    PKT_CFG_CLIENT_INFO = 0x0,
    PKT_CFG_COOKIE_RESPONSE = 0x1,
    PKT_CFG_CUSTOM_SERVER = 0x2,
    PKT_CFG_FINISH_ACK = 0x3,
    PKT_CFG_SERVER_KEEP_ALIVE = 0x4,
    PKT_CFG_PONG = 0x5,
    PKT_CFG_RESPACK_RESPONSE = 0x6,
    PKT_CFG_KNOWN_DATAPACKS_SERVER = 0x7,
    /**@}*/

    _PKT_TYPE_COUNT = 0x11
};

/**
 * Base structure of a packet.
 *
 * All packets share this structure.
 * The @ref pkt#payload field is only used internally. It is not sent to clients.
 */
typedef struct Packet {
    enum PacketType id; /**< The packet identifier. */
    u32 total_length;   /**< The length of the payload and packet ID (as a VarInt). Only used to
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

typedef struct {
    JSON reason;
} PacketLoginDisconnect;

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
} PacketCryptRequest;

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
} PacketCryptResponse;

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

/* === CONFIGURATION === */

typedef struct {
    ResourceID channel;
    u8* data;
    u16 data_length;
} PacketCustom;

enum ChatMode {
    CHAT_ENABLED = 0,
    CHAT_COMMANDS = 1,
    CHAT_HIDDEN = 2,
};

enum MainHand {
    HAND_LEFT = 0,
    HAND_RIGHT = 1,
};

enum ParticleStatus {
    PARTICLE_ALL = 0,
    PARTICLE_DECREASED = 1,
    PARTICLE_MINIMAL = 2,
};

typedef struct {
    string locale;
    i8 render_distance;
    enum ChatMode chat_mode;
    bool chat_colors;
    u8 skin_part_mask;
    enum MainHand hand;
    bool text_filtering;
    bool allow_server_listings;
    enum ParticleStatus particle_status;
} PacketClientInfo;

typedef struct {
    Vector features;
} PacketSetFeatureFlags;

typedef struct KnownDatapack {
    string namespace;
    string id;
    string version;
} KnownDatapack;

typedef struct {
    Vector known_packs; // KnownPack
} PacketKnownDatapacks;

typedef struct RegistryDataEntry {
    ResourceID id;
    NBT data;
} RegistryDataEntry;

typedef struct {
    ResourceID registry_id;
    Vector entries; // RegistryDataEntry
} PacketRegistryData;

typedef struct TagEntry {
    ResourceID id;
    Vector elements; // i32
} TagEntry;

typedef struct TagRegistryData {
    ResourceID registry_id;
    Vector entries; // TagEntry
} TagRegistryData;

typedef struct {
    Vector tags; // TagRegistryData
} PacketUpdateTags;

#endif /* ! PACKET_H */

/** @} */
