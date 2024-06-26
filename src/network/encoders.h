#ifndef ENCODERS_H
#define ENCODERS_H

#include "containers/bytebuffer.h"
#include "packet.h"

void pkt_encode_status(const Packet* pkt, ByteBuffer* buffer);
void pkt_encode_ping(const Packet* pkt, ByteBuffer* buffer);

#endif /* ! ENCODERS_H */
