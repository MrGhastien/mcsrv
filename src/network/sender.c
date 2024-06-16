#include "sender.h"
#include "utils/bitwise.h"
#include "bytebuffer.h"
#include "network.h"
#include "packet.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

static enum IOCode send_bytebuf(ByteBuffer* buffer, int sockfd) {
    if(buffer->size == 0)
        return IOC_OK;

    u64 sent;
    enum IOCode code = try_send(sockfd, buffer->buf, buffer->size, &sent);
    u64 remaining = buffer->size - sent;

    memmove(buffer->buf, offset(buffer->buf, sent), remaining);
    buffer->size = remaining;
    return code;
}

void write_packet(const Packet* pkt, Connection* conn) {
    pkt_encoder encoder = get_pkt_encoder(pkt, conn);
    u64 initial_length = conn->send_buffer.size;

    bytebuf_write_varint(&conn->send_buffer, pkt->id);
    encoder(pkt, &conn->send_buffer);
    
    u64 pkt_length = conn->send_buffer.size - initial_length;
    u8 pkt_length_buf[4];
    u64 pkt_length_size = encode_varint(pkt_length, pkt_length_buf);

    bytebuf_reserve(&conn->send_buffer, pkt_length_size);
    memmove(offset(conn->send_buffer.buf, pkt_length_size), conn->send_buffer.buf, pkt_length);
    memcpy(conn->send_buffer.buf, pkt_length_buf, pkt_length_size);

    puts("====");
    puts("Sending packet:");
    printf("  - Size: %zu\n  - ID: %u\n", pkt_length, pkt->id);

    // Send bytes ignoring the first ones
    send_bytebuf(&conn->send_buffer, conn->sockfd);
}

enum IOCode sender_send(Connection* conn) {
    return send_bytebuf(&conn->send_buffer, conn->sockfd);
}
