/**
 * @file
 *
 * Functions which do some action(s) when a packet is received.
 *
 * Handling is the third and last step done when receiving a packet.
 * After packets were read and decoded, another function is used to
 * execute some actions (e.g. trigger events, update a specific sub-system...)
 * specific to a packet type and a connection state.
 * Such functions are declared here.
 *
 * @see decoders.h
 * @see encoders.h
 */
#ifndef HANDLER_H
#define HANDLER_H

#include "network/connection.h"
#include "packet.h"

/**
 * Helper macro used to declare handling functions easily.
 * Handling functions follow a specific naming convention: `pkt_handle_<name>` and
 * always have 2 arguments:
 * - [in] `packet`: The received packet to handle.
 * - `bytes`: The connection trough which the packet was received.
 *
 * Handling functions return @ref TRUE when the packet was successfully handled,
 * @ref FALSE othwerwise.
 *
 * @param name The name of the handler. Should be somewhat similar to the type of handled packets.
 */
#define PKT_HANDLER(name) bool pkt_handle_##name(NetworkContext* ctx, const Packet* pkt, Connection* conn)

PKT_HANDLER(dummy);

PKT_HANDLER(handshake);

PKT_HANDLER(status);
PKT_HANDLER(ping);

PKT_HANDLER(log_start);
PKT_HANDLER(enc_res);

#endif /* ! HANDLER_H */
