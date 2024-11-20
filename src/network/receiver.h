/**
 * @file
 *
 * Part of the network subsystem thats receives and handles packets.
 */
#ifndef RECEIVER_H
#define RECEIVER_H

#include "connection.h"

/**
 * Reads, decodes, and handles a single packet from the given connection.
 *
 * @param[in] conn The connection to read from
 * @return A code indicating the current IO status:
 *         - @ref IOC_OK if a packet was read successfully in its entirety,
 *         - @ref IOC_AGAIN if the receiver could not finish reading a packet without waiting for IO,
 *         - @ref IOC_PENDING if the receiver has started IO operations which could not be completed immediately,
 *         - @ref IOC_ERROR if a reading error occurred,
 *         - @ref IOC_CLOSED if the connection was closed while reading a packet.
 */
enum IOCode receive_packet(NetworkContext* ctx, Connection* conn);

#endif /* ! RECEIVER_H */
