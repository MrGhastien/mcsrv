#ifndef CONTEXT_H
#define CONTEXT_H

#include "definitions.h"
#include "memory/arena.h"
#include "containers/bytebuffer.h"
#include "network/compression.h"
#include "network/encryption.h"
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

typedef struct connection {
    /** Arena used to allocate data which persists as long as the connection*/
    Arena persistent_arena;
    /** Arena used to allocate temporary data for sending / receiving packets */
    Arena scratch_arena;

    bool compression;
    bool encryption;
    enum State state;

    EncryptionContext* global_enc_ctx;
    PeerEncryptionContext peer_enc_ctx;
    CompressionContext cmprss_ctx;
    
    int sockfd;

    /** These 4 fields are used to keep track of the reading progress */
    bool has_read_size;
    u8* pkt_buffer;
    u64 bytes_read;
    u64 buffer_size;

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

Connection conn_create(int sockfd, u64 table_index, EncryptionContext* enc_ctx, string addr, u32 port);

void conn_reset_buffer(Connection* conn, void* new_buffer, u64 size);

enum IOCode conn_read_bytes(Connection* conn);
bool conn_is_resuming_read(const Connection* conn);

enum IOCode conn_send_bytes(Connection* conn);

#endif /* ! CONTEXT_H */
