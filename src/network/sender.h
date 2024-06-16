#ifndef SENDER_H
#define SENDER_H

#include "connection.h"

void write_packet(const Packet* pkt, Connection* conn);
enum IOCode sender_send(Connection* conn);

#endif /* ! SENDER_H */
