//
// Created by bmorino on 15/11/2024.
//

#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include "memory/arena.h"

#define IOEVENT_IN 1
#define IOEVENT_OUT 2

/**
 * Enumeration used internally to keep track of the state of connections.
 * This is notably used to report and propagate errors to close connections
 * gracefully.
 */
enum IOCode {
    IOC_OK,    /**< No errors occurred, connection can stay open. */
    IOC_ERROR, /**< An error occurred, the connection must be closed. */
    IOC_AGAIN, /**< Data could not be read all at once, we should try reading more when possible. */
    IOC_CLOSED /**< The connection has been closed by the peer, stop handling it. */
};

typedef struct NetworkContext {
    Arena arena;

    Connection* connections;
    u32 max_connections;
    u32 connection_count;

    int serverfd;
    int epollfd;
    int eventfd;
    MCThread thread;

    EncryptionContext enc_ctx;
    u64 compress_threshold;

    string host;
    u32 port;

    i32 code;
    bool should_continue;
} NetworkContext;

#endif /* ! COMMON_TYPES_H */
