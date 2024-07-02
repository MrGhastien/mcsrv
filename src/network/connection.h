#ifndef CONTEXT_H
#define CONTEXT_H

#include "definitions.h"
#include "memory/arena.h"
#include "containers/bytebuffer.h"
#include "network/compression.h"
#include "network/encryption.h"

typedef struct pkt Packet;

enum State {
    STATE_CLOSED = -1,
    STATE_HANDSHAKE,
    STATE_STATUS,
    STATE_LOGIN,
    STATE_PLAY,
    _STATE_COUNT
};

typedef struct {
    Arena persistent_arena;
    Arena scratch_arena;
    bool compression;
    bool encryption;
    enum State state;

    EncryptionContext* global_enc_ctx;
    PeerEncryptionContext peer_enc_ctx;

    CompressionContext cmprss_ctx;
    
    int sockfd;

    // Current reading progress
    bool has_read_size;
    u8* pkt_buffer;
    u64 bytes_read;
    u64 buffer_size;

    ByteBuffer send_buffer;

    u64 verify_token_size;
    u8* verify_token;

    u64 table_index;

    pthread_mutex_t mutex;
} Connection;

Connection conn_create(int sockfd, u64 table_index, EncryptionContext* enc_ctx);

void conn_reset_buffer(Connection* conn, void* new_buffer, u64 size);

enum IOCode conn_read_bytes(Connection* conn);
bool conn_is_resuming_read(const Connection* conn);

enum IOCode conn_send_bytes(Connection* conn);

#endif /* ! CONTEXT_H */
