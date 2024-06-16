#ifndef CONTEXT_H
#define CONTEXT_H

#include "../definitions.h"
#include "../memory/arena.h"
#include "bytebuffer.h"
#include <sys/types.h>

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
    Arena arena;
    bool compression;
    enum State state;
    int sockfd;

    // Current reading progress
    bool has_read_size;
    u8* pkt_buffer;
    u64 bytes_read;
    u64 buffer_size;

    ByteBuffer send_buffer;

    u64 table_index;
} Connection;

Connection conn_create(int sockfd, u64 table_index);

void conn_reset_buffer(Connection* conn, void* new_buffer, size_t size);

enum IOCode conn_read_bytes(Connection* conn);
bool conn_is_resuming_read(const Connection* conn);

enum IOCode conn_send_bytes(Connection* conn);

#endif /* ! CONTEXT_H */
