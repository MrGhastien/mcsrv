#ifndef PACKET_H
#define PACKET_H

#include "../definitions.h"
#include "containers/bytebuffer.h"
#include "connection.h"
#include "../memory/arena.h"
#include "../utils/string.h"
#include <stddef.h>

enum PacketType {
    PKT_HANDSHAKE = 0x0,
    PKT_STATUS = 0x0,
    PKT_PING = 0x1,
};

typedef struct raw_pkt {
    void* data;
    size_t size;
} RawPacket;

typedef struct pkt {
    enum PacketType id;
    size_t total_length;
    size_t payload_length;
    void* payload;
} Packet;

typedef struct {
    int protocol_version;
    string srv_addr;
    u16 srv_port;
    int next_state;
} PacketHandshake;

typedef struct {
    string data;
} PacketStatusResponse;

typedef struct {
    long num;
} PacketPing;

typedef void (*pkt_acceptor)(const Packet* pkt, Connection* conn);
typedef void (*pkt_decoder)(Packet* pkt, Arena* arena, u8* raw);
typedef void (*pkt_encoder)(const Packet* pkt, ByteBuffer* buffer);

pkt_acceptor get_pkt_handler(const Packet* pkt, Connection* conn);
pkt_decoder get_pkt_decoder(const Packet* pkt, Connection* conn);
pkt_encoder get_pkt_encoder(const Packet* pkt, Connection* conn);

#endif /* ! PACKET_H */
