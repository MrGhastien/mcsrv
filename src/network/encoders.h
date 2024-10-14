/**
 * @file
 *
 * Functions which transform a @ref Packet structure into raw binary data.
 *
 * Encoding is the first step done when sending a packet.
 *
 * Other sub-systems can request a packet to be sent. When a packet has to be sent,
 * it is first encoded, i.e. transformed into a stream of raw binary data. A function
 * specific to the packet's type and the state of the connection is used for this purpose.
 * Such functions are declared here.
 *
 * @see decoders.h
 * @see handlers.h
 */
#ifndef ENCODERS_H
#define ENCODERS_H

#include "containers/bytebuffer.h"
#include "packet.h"

/**
 * Helper macro used to declare encoding functions easily.
 * Encoding functions follow a specific name convention: `pkt_encode_<name>` and
 * always have 2 arguments:
 * - [in] `packet`: The packet structure to encode.
 * - [out] `bytes`: A byte buffer in which the encoded packet in written.
 *
 * Decoding functions have no return value.
 *
 * @param name The name of the encoder. Should be somewhat similar to the type of encoded packets.
 */
#define PKT_ENCODER(name) void pkt_encode_##name(const Packet* pkt, ByteBuffer* buffer)

PKT_ENCODER(dummy);

PKT_ENCODER(status);
PKT_ENCODER(ping);

PKT_ENCODER(enc_req);
PKT_ENCODER(compress);
PKT_ENCODER(log_success);

#endif /* ! ENCODERS_H */
