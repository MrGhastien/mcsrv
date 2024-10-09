/**
 * @file network.h
 * @author Bastien Morino
 * @ingroup networking
 * @addtogroup networking
 * @{
 *
 * General functions regarding the Network sub-system.
 */

#ifndef NETWORK_H
#define NETWORK_H

#include "definitions.h"

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
