/**
 * @file
 *
 * Functions which read a byte buffer and populate a @ref Packet structure.
 *
 * Decoding is the second step done when receiving a packet.
 * After packets were read through the socket, a function specific to the packet's
 * type and the state of the connection is used to transform the raw binary data
 * into a usable @ref Packet structure.
 * Those functions are declared here.
 *
 * @see encoders.h
 * @see handlers.h
 */
#ifndef DECODERS_H
#define DECODERS_H

#include "connection.h"
#include "packet.h"

#define PKT_DECODER(name) pkt_decode_##name

/**
 * Helper macro used to declare decoding functions easily.
 * Decoding functions follow a specific name convention: `pkt_decode_<name>` and
 * always have 3 arguments:
 * - [out] `packet`: The packet structure to populate.
 * - `arena`: An arena used to allocate packet payloads (and other stuff if needed).
 * - [in] `bytes`: A byte buffer containing the raw bytes of the packet.
 *
 * Decoding functions have no return value.
 *
 * @param name The name of the decoder. Should be somewhat similar to the type of decoded packets.
 */
#define DEF_PKT_DECODER(name) void PKT_DECODER(name)(Packet* packet, Arena* arena, ByteBuffer* bytes)

DEF_PKT_DECODER(dummy);

/* === HANDSHAKE === */
DEF_PKT_DECODER(handshake);

/* === STATUS === */
DEF_PKT_DECODER(ping);

/* === LOGIN === */
DEF_PKT_DECODER(login_start);
DEF_PKT_DECODER(crypt_response);
DEF_PKT_DECODER(cfg_custom_server);

/* === CONFIGURATION === */
DEF_PKT_DECODER(cfg_client_info);
DEF_PKT_DECODER(cfg_custom);
DEF_PKT_DECODER(cfg_finish_config_ack);
DEF_PKT_DECODER(cfg_keep_alive);
DEF_PKT_DECODER(cfg_respack_response);
DEF_PKT_DECODER(cfg_known_datapacks);

#endif /* ! DECODERS_H */
