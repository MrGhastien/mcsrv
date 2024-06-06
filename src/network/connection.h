#ifndef CONTEXT_H
#define CONTEXT_H

#include "../definitions.h"
#include "../memory/arena.h"

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
} Connection;


#endif /* ! CONTEXT_H */
