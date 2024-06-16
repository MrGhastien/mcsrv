#ifndef NETWORK_H
#define NETWORK_H

#include "../definitions.h"

enum IOCode {
    IOC_OK,
    IOC_ERROR,
    IOC_AGAIN,
    IOC_CLOSED
};

int net_handle(char* host, int port, u64 max_connections);

#endif /* ! NETWORK_H */
