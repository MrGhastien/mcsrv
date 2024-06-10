#ifndef ENCODERS_H
#define ENCODERS_H

#include "packet.h"

void pkt_encode_status(const Packet* pkt, Connection* conn, Arena* arena);
void pkt_encode_ping(const Packet* pkt, Connection* conn, Arena* arena);

#endif /* ! ENCODERS_H */
