#ifndef HANDLER_H
#define HANDLER_H

#include "packet.h"

void pkt_handle_handshake(Packet* pkt, Connection* conn);

void pkt_handle_status(Packet* pkt, Connection* conn);

void pkt_handle_ping(Packet* pkt, Connection* conn);

#endif /* ! HANDLER_H */
