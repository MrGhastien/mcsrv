/**
 * @file
 *
 * Part of the network subsystem that encodes and sends packets. 
 */

#ifndef SENDER_H
#define SENDER_H

#include "connection.h"

/**
 * Encodes and tries to send a packet immediately.
 *
 * If it is not possible to send all of the packet's bytes without waiting,
 * the remaining bytes are put in the connection's sending queue.
 *
 * Packets are also compressed and/or encrypted if enabled.
 * @param[in] pkt The packet to send.
 * @param[in] conn The connection to send a packet through.
 */
void write_packet(const Packet* pkt, Connection* conn);
/**
 * Tries sending buffered bytes through the given connection.
 *
 * @param[in] connection The connection to send byte through.
 * @return
 *         - @ref IOC_OK if all buffered bytes were sent,
 *         - @ref IOC_AGAIN if the sender could not finish sending all of the remaining bytes without waiting for I/O,
 *         - @ref IOC_ERROR if an error occurred while sending,
 *         - @ref IOC_CLOSED if the connection was closed while sending bytes.
 */
enum IOCode sender_send(Connection* conn);

#endif /* ! SENDER_H */
