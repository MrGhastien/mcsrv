#ifndef PACKET_H
#define PACKET_H

#include "definitions.h"
#include "connection.h"
#include <stddef.h>

enum PacketType {
    PKT_HANDSHAKE = 0x0,
    PKT_STATUS = 0x0,
    PKT_PING = 0x1,
};

typedef struct {
    int id;
    size_t total_length;
    size_t payload_length;
    void* payload;
} Packet;

typedef struct {
    int protocol_version;
    char* srv_addr;
    u16 srv_port;
    int next_state;
} PacketHandshake;

typedef void (*pkt_acceptor)(Packet* pkt, Connection* conn);
typedef void (*pkt_decoder)(Packet* pkt, const Connection* conn, u8* raw);

pkt_acceptor get_pkt_handler(Packet* pkt, Connection* conn);
pkt_decoder get_pkt_decoder(Packet* pkt, const Connection* conn);

Packet* packet_read(const Connection* conn);

void packet_decode_handshake(Packet* packet, const Connection* ctx);

#endif /* ! PACKET_H */
