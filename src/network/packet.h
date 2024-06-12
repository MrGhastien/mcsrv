#ifndef PACKET_H
#define PACKET_H

#include "../definitions.h"
#include "connection.h"
#include "../memory/arena.h"
#include "../utils/string.h"
#include <stddef.h>

enum PacketType {
    PKT_HANDSHAKE = 0x0,
    PKT_STATUS = 0x0,
    PKT_PING = 0x1,
};

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

typedef void (*pkt_acceptor)(Packet* pkt, Connection* conn);
typedef void (*pkt_decoder)(Packet* pkt, Connection* conn, u8* raw);
typedef void (*pkt_encoder)(const Packet* pkt, Connection* conn, Arena* buffer_arena);

pkt_acceptor get_pkt_handler(Packet* pkt, Connection* conn);
pkt_decoder get_pkt_decoder(Packet* pkt, Connection* conn);

/**
   Read and decode a packet from the given connection.
 */
enum IOCode packet_read(Connection* conn);
bool packet_decode(Connection* conn, Packet* out_pkt);
enum IOCode packet_write(const Packet* pkt, Connection* conn, pkt_encoder encoder);

#endif /* ! PACKET_H */
