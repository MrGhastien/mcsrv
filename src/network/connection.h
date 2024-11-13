/**
 * @file connection.h
 * @author Bastien Morino
 *
 * @brief Structure representing a connection.
 */

#ifndef CONTEXT_H
#define CONTEXT_H

#include "containers/bytebuffer.h"
#include "definitions.h"
#include "memory/arena.h"
#include "network/compression.h"
#include "network/security.h"
#include "utils/string.h"

#include "platform/mc_mutex.h"

typedef struct Packet Packet;

/**
 * Enumeration of connection states.
 *
 * 
 */
enum State {
    /** The connection is closed. */
    STATE_CLOSED = -1,
    /** The connection is open, and no packets have been received yet. */
    STATE_HANDSHAKE,
    /** The connection is open to send the server's ping. */
    STATE_STATUS,
    /** The connected peer is logging-in. */
    STATE_LOGIN,
    /** The connected peer is playing the game. */
    STATE_PLAY,
    _STATE_COUNT
};

/**
 * Represents a connection to a peer.
 *
 * Contains the state of a connection, the OS specific socket handles, and buffers to keep
 * track of data to read / write.
 * Connections for status requests, i.e. server pings in the multiplayer menu, do not have
 * a player name, packet encryption or compression.
 */
typedef struct Connection {
    /** Arena used to allocate data which persists as long as the connection. */
    Arena persistent_arena;
    /** Arena used to allocate temporary data, e.g. for sending / receiving packets. */
    Arena scratch_arena;

    bool compression; /**< Whether packet compression is enabled. */
    bool encryption;  /**< Whether packet encryption is enabled. */
    enum State state; /**< The current PC protocol state of the connection. */

    /** Pointer to the encryption context used by all connections. */
    EncryptionContext* global_enc_ctx;
    PeerEncryptionContext peer_enc_ctx; /**< Peer-specific encryption context. */
    CompressionContext cmprss_ctx;      /**< Compression context. */

    int sockfd; /**< Linux file descriptor of the connection's socket.*/

    /** Keeps track of whether the last packet read operation read the packet's size or not. */
    bool has_read_size; 
    /** Buffer storing raw (possibly compressed or encrypted) packets. */
    ByteBuffer recv_buffer;
    /** Keeps track of whether the underlying socket is writable or not. */
    bool can_send;
    /** Encoded packet sending queue */
    ByteBuffer send_buffer;

    u64 verify_token_size;
    u8* verify_token;

    /** Index of the connection in the connection table */
    u64 table_index;

    /** Name of the player connected to the server. */
    string player_name;
    string peer_addr; /**< Address of the connected peer represented by this connection. */
    u32 peer_port; /**< TCP port of the connected peer. */

    /** Thread MutEx device to prevent race conditions. */
    MCMutex mutex;
} Connection;

/**
   * @brief Initializes a new connection.
 *
 * @param sockfd The File Descriptor of the socket, connected to the peer.
 * @param table_index The index of the new connection in the global connection table.
 * @param[in] enc_ctx Pointer to the global encryption context.
 * @param addr The address of the connected peer.
 * @param port the TCP port through which the peer is connected.
* @return A new connection.
 */
Connection
conn_create(int sockfd, u64 table_index, EncryptionContext* enc_ctx, string addr, u32 port);

/**
 * Indicates whether a previous packet read was stopped.
 *
 * When reading from the socket of the provided connection would block, the reading process
 * is interrupted and the network thread goes back to waiting for EPoll events.
 * In that case, when the socket becomes readable again, we should *resume* the reading
 * process where it was stopped.
 *
 * @param[in] conn The connection to check the reading process of.
 * @return @ref TRUE if the previous read was interrupted, @ref FALSE otherwise.
 */
bool conn_is_resuming_read(const Connection* conn);

/**
 * Indicates whether a connection is closed or open.
 *
 * @param[in] conn The connection to check.
 * @return @ref TRUE if the connection is closed, @ref FALSE otherwise.
 */
bool conn_is_closed(const Connection* conn);

#endif /* ! CONTEXT_H */
