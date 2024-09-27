/**
 * @file connection.h
 * @author Bastien Morino
 *
 */

#ifndef CONTEXT_H
#define CONTEXT_H

#include "containers/bytebuffer.h"
#include "definitions.h"
#include "memory/arena.h"
#include "network/compression.h"
#include "network/security.h"
#include "utils/string.h"

typedef struct pkt Packet;

enum State {
    STATE_CLOSED = -1,
    STATE_HANDSHAKE,
    STATE_STATUS,
    STATE_LOGIN,
    STATE_PLAY,
    _STATE_COUNT
};

/**
 * @brief Represents the state of a connection.
 * Contains the state of a connection, and buffers to keep track of data to read / write.
 */
typedef struct connection {
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

    bool has_read_size;
    ByteBuffer recv_buffer;
    /** Keeps track of whether the underlying socket is writable or not. */
    bool can_send;
    /** Encoded packet sending queue */
    ByteBuffer send_buffer;

    u64 verify_token_size;
    u8* verify_token;

    /** Index of the connection in the connection table */
    u64 table_index;

    string player_name;
    string peer_addr;
    u32 peer_port;

    pthread_mutex_t mutex;
} Connection;

Connection
conn_create(int sockfd, u64 table_index, EncryptionContext* enc_ctx, string addr, u32 port);

bool conn_is_resuming_read(const Connection* conn);

bool conn_is_closed(const Connection* conn);

#endif /* ! CONTEXT_H */
