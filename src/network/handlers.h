#ifndef HANDLER_H
#define HANDLER_H

#include "network/connection.h"
#include "packet.h"

bool pkt_handle_dummy(const Packet* pkt, Connection* conn);

bool pkt_handle_handshake(const Packet* pkt, Connection* conn);

bool pkt_handle_status(const Packet* pkt, Connection* conn);
bool pkt_handle_ping(const Packet* pkt, Connection* conn);

bool pkt_handle_log_start(const Packet* pkt, Connection* conn);

#endif /* ! HANDLER_H */
