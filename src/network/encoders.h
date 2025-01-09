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

#define PKT_ENCODER(name) pkt_encode_##name

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
#define DEF_PKT_ENCODER(name) void PKT_ENCODER(name)(const Packet* pkt, ByteBuffer* buffer)

DEF_PKT_ENCODER(dummy);

/* === STATUS === */
DEF_PKT_ENCODER(status);
DEF_PKT_ENCODER(ping);

/* === LOGIN === */
DEF_PKT_ENCODER(crypt_request);
DEF_PKT_ENCODER(compress);
DEF_PKT_ENCODER(login_success);

/* === CONFIGURATION === */
DEF_PKT_ENCODER(cfg_custom);
DEF_PKT_ENCODER(cfg_disconnect);
DEF_PKT_ENCODER(cfg_keep_alive);
DEF_PKT_ENCODER(cfg_ping);
DEF_PKT_ENCODER(cfg_registry_data);
DEF_PKT_ENCODER(cfg_remove_respack);
DEF_PKT_ENCODER(cfg_add_respack);
DEF_PKT_ENCODER(cfg_transfer);
DEF_PKT_ENCODER(cfg_set_feature_flags);
DEF_PKT_ENCODER(cfg_update_tags);
DEF_PKT_ENCODER(cfg_known_datapacks);
DEF_PKT_ENCODER(cfg_custom_report);
DEF_PKT_ENCODER(cfg_server_links);

#endif /* ! ENCODERS_H */
