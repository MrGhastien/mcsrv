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

#define PKT_HANDLER(name) pkt_handle_##name

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
#define DEF_PKT_HANDLER(name) bool PKT_HANDLER(name)(const Packet* pkt, Connection* conn)

DEF_PKT_HANDLER(dummy);

/* === HANDSHAKE === */
DEF_PKT_HANDLER(handshake);

/* === STATUS === */
DEF_PKT_HANDLER(status);
DEF_PKT_HANDLER(ping);

/* === LOGIN === */
DEF_PKT_HANDLER(login_start);
DEF_PKT_HANDLER(crypt_response);
DEF_PKT_HANDLER(login_ack);

/* === CONFIGURATION === */
DEF_PKT_HANDLER(cfg_client_info);
DEF_PKT_HANDLER(cfg_custom);
DEF_PKT_HANDLER(cfg_finish_config_ack);
DEF_PKT_HANDLER(cfg_keep_alive);
DEF_PKT_HANDLER(cfg_pong);
DEF_PKT_HANDLER(cfg_respack_response);
DEF_PKT_HANDLER(cfg_known_datapacks);

#endif /* ! HANDLER_H */
