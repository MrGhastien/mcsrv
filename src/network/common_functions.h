//
// Created by bmorino on 18/11/2024.
//

#ifndef COMMON_FUNCTIONS_H
#define COMMON_FUNCTIONS_H

#include "definitions.h"
#include "network/connection.h"

void network_finish(void);

i32 handle_connection_io(Connection* conn, u32 events);

#endif /* ! COMMON_FUNCTIONS_H */
