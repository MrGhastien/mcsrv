#ifndef HANDLER_H
#define HANDLER_H

#include "packet.h"

void pkt_handle_handshake(const Packet* pkt, Connection* conn);

void pkt_handle_status(const Packet* pkt, Connection* conn);

void pkt_handle_ping(const Packet* pkt, Connection* conn);

#endif /* ! HANDLER_H */
