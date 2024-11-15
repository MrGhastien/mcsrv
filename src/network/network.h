/**
 * @file network.h
 * @author Bastien Morino
 * @ingroup networking
 *
 * General functions regarding the Network sub-system.
 *
 * @addtogroup networking
 * @{
 */

#ifndef NETWORK_H
#define NETWORK_H

#include "definitions.h"

/**
 * Initializes the network sub-system and sets the server's host, port and
 * maximum amount of simultaneous connections.
 *
 * @param[in] host The host IPv4 address to use.
 * @param[in] port The TCP port the server will listen to.
 * @param[in] max_connections The maximum amount of simultaneous connections to the server.
 *
 * @return Zero if the sub-system was initialized correctly, else a non-zero error code.
 */
i32 network_init(char* host, i32 port, u64 max_connections);

/**
 * Stops the network sub-system.
 */
void network_stop(void);

#endif /* ! NETWORK_H */

/** @} */
