#ifndef DECODERS_H
#define DECODERS_H

#include "connection.h"
#include "packet.h"

void pkt_decode_dummy(Packet* packet, Arena* arena, u8* raw);

void pkt_decode_handshake(Packet* packet, Arena* arena, u8* raw);
void pkt_decode_ping(Packet* packet, Arena* arena, u8* raw);

void pkt_decode_log_start(Packet* packet, Arena* arena, u8* raw);

#endif /* ! DECODERS_H */
