#ifndef NETWORK_H
#define NETWORK_H

#include "definitions.h"

enum IOCode {
    IOC_OK,
    IOC_ERROR,
    IOC_AGAIN,
    IOC_CLOSED
};

i32 network_init(char* host, i32 port, u64 max_connections);
void network_stop(void);

#endif /* ! NETWORK_H */
