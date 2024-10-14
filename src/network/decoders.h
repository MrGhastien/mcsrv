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
#define PKT_DECODER(name) void pkt_decode_##name(Packet* packet, Arena* arena, ByteBuffer* bytes)

PKT_DECODER(dummy);

PKT_DECODER(handshake);
PKT_DECODER(ping);

PKT_DECODER(log_start);
PKT_DECODER(enc_res);

#endif /* ! DECODERS_H */
