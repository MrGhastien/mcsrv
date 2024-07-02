#ifndef HANDLER_H
#define HANDLER_H

#include "network/connection.h"
#include "packet.h"

#define PKT_HANDLER(name) bool pkt_handle_##name(const Packet* pkt, Connection* conn)

PKT_HANDLER(dummy);

PKT_HANDLER(handshake);

PKT_HANDLER(status);
PKT_HANDLER(ping);

PKT_HANDLER(log_start);
PKT_HANDLER(enc_res);

#endif /* ! HANDLER_H */
