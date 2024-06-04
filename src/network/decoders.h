#ifndef DECODERS_H
#define DECODERS_H

#include "connection.h"
#include "packet.h"

void pkt_decode_dummy(Packet* packet, const Connection* conn, u8* raw);

void pkt_decode_handshake(Packet* packet, const Connection* conn, u8* raw);

#endif /* ! DECODERS_H */
