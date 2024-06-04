#ifndef CONTEXT_H
#define CONTEXT_H

#include "definitions.h"

enum State {
    STATE_CLOSED = -1,
    STATE_HANDSHAKE,
    STATE_STATUS,
    STATE_LOGIN,
    STATE_PLAY,
    _STATE_COUNT
};

typedef struct {
    bool compression;
    enum State state;
    int sockfd;
} Connection;


#endif /* ! CONTEXT_H */
