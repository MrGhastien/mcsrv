#include "sender.h"
#include "logger.h"
#include "utils/bitwise.h"
#include "bytebuffer.h"
#include "network.h"
#include "packet.h"
#include "utils.h"

#include <pthread.h>
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

static void insert_into_bytebuf(ByteBuffer* buffer, u64 pos, const void* data, u64 size) {
    bytebuf_reserve(buffer, size);
    void* src = offset(buffer->buf, pos);
    void* dst = offset(src, size);
    memmove(dst, src, buffer->size - pos);
    memcpy(src, data, size);
}

void write_packet(const Packet* pkt, Connection* conn) {
    pkt_encoder encoder = get_pkt_encoder(pkt, conn);
    u64 initial_length = conn->send_buffer.size;

    pthread_mutex_lock(&conn->mutex);

    bytebuf_write_varint(&conn->send_buffer, pkt->id);
    encoder(pkt, &conn->send_buffer);
    
    u64 pkt_length = conn->send_buffer.size - initial_length;
    u8 pkt_length_buf[4];
    u64 pkt_length_size = encode_varint(pkt_length, pkt_length_buf);

    log_tracef("Sending: { Size: %zu, ID: %zu }", pkt_length, pkt->id);

    insert_into_bytebuf(&conn->send_buffer, initial_length, pkt_length_buf, pkt_length_size);
    send_bytebuf(&conn->send_buffer, conn->sockfd);

    pthread_mutex_unlock(&conn->mutex);
}

enum IOCode sender_send(Connection* conn) {
    pthread_mutex_lock(&conn->mutex);

    enum IOCode code = send_bytebuf(&conn->send_buffer, conn->sockfd);

    pthread_mutex_unlock(&conn->mutex);
    return code;
}
