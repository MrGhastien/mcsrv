#ifndef DECODERS_H
#define DECODERS_H

#include "connection.h"
#include "packet.h"

#define PKT_DECODER(name) void pkt_decode_##name(Packet* packet, Arena* arena, ByteBuffer* bytes)

PKT_DECODER(dummy);

PKT_DECODER(handshake);
PKT_DECODER(ping);

PKT_DECODER(log_start);
PKT_DECODER(enc_res);

#endif /* ! DECODERS_H */
