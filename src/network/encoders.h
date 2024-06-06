#ifndef ENCODERS_H
#define ENCODERS_H

#include "packet.h"

void pkt_encode_status(const Packet* pkt, Connection* conn);

#endif /* ! ENCODERS_H */
