#ifndef ENCODERS_H
#define ENCODERS_H

#include "containers/bytebuffer.h"
#include "packet.h"

#define PKT_ENCODER(name) void pkt_encode_##name(const Packet* pkt, ByteBuffer* buffer)

PKT_ENCODER(dummy);

PKT_ENCODER(status);
PKT_ENCODER(ping);

PKT_ENCODER(enc_req);
PKT_ENCODER(compress);

#endif /* ! ENCODERS_H */
