#ifndef CONTEXT_H
#define CONTEXT_H

#include "../definitions.h"
#include "../memory/arena.h"
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

enum IOCode {
    IOC_OK,
    IOC_ERROR,
    IOC_AGAIN
};

typedef struct {
    Arena arena;
    bool compression;
    enum State state;
    int sockfd;

    bool size_read;
    u8* pkt_buffer;
    size_t bytes_read;
    size_t buffer_size;

    Arena send_arena;
} Connection;

Connection conn_create(int sockfd);
void conn_reset_buffer(Connection* conn, void* new_buffer, size_t size);

enum IOCode conn_read_bytes(Connection* conn);
bool conn_is_resuming_read(const Connection* conn);

enum IOCode conn_send_bytes(Connection* conn);

#endif /* ! CONTEXT_H */
