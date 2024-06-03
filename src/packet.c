#include "packet.h"
#include "context.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

static size_t read_varint(const Connection* conn, int* out) {
    int res = 0;
    int status;
    size_t position = 0;
    size_t size = 0;
    u8 byte = CONTINUE_BIT;
    while (TRUE) {
        status = recv(conn->sockfd, &byte, sizeof byte, 0);

        res |= (byte & SEGMENT_BITS) << position;

        position += 7;
        size++;

        if ((byte & CONTINUE_BIT) == 0)
            break;

        if (position >= 32)
            return -1;
    }

    if (status == -1) {
        perror("Failed to read varint");
        return -1;
    }

    *out = res;
    return size;
}

static int pkt_decode_varint(Packet* packet, const Connection* ctx) {
    int res;
    size_t size = decode_varint(packet->raw + packet->read, &res);
    packet->read += size;
    return res;
}

char* pkt_decode_string(Packet* packet, const Connection* ctx) {
    int length = pkt_decode_varint(packet, ctx);
    if (length <= 0)
        return NULL;

    char* str = malloc(length + 1);
    if (!str)
        return NULL;
    str[length] = 0;

    for (size_t i = 0; i < length; i++) {
        str[i] = packet->raw[packet->read];
        packet->read++;
    }
    return str;
}

u16 pkt_decode_u16(Packet* pkt, const Connection* ctx) {
    u16 res = (pkt->raw[pkt->read] << 8) | pkt->raw[pkt->read + 1];
    pkt->read += 2;
    return res;
}

void packet_decode_handshake(Packet* packet, const Connection* ctx) {

    PacketHandshake* hshake = malloc(sizeof(PacketHandshake));
    if (!hshake)
        return;

    hshake->protocol_version = pkt_decode_varint(packet, ctx);
    hshake->srv_addr = pkt_decode_string(packet, ctx);
    hshake->srv_port = pkt_decode_u16(packet, ctx);
    hshake->next_state = pkt_decode_varint(packet, ctx);

    packet->decoded = hshake;
}

Packet* packet_read(const Connection* conn) {
    int length;
    int id;
    if (read_varint(conn, &length) == -1)
        return NULL;

    size_t data_size = read_varint(conn, &id);
    if (data_size == -1)
        return NULL;

    data_size = length - data_size;

    Packet* pkt = malloc(sizeof *pkt + data_size);

    pkt->total_length = length;
    pkt->payload_length = data_size;
    pkt->id = id;
    pkt->read = 0;

    socket_readbytes(conn->sockfd, pkt->raw, data_size);
    if (conn->state == STATE_HANDSHAKE) {
        packet_decode_handshake(pkt, conn);
    }
    pkt_decoder decoder = get_pkt_decoder(pkt, conn);
    decoder(pkt, conn);
    return pkt;
}
