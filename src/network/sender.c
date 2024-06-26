#include "sender.h"
#include "containers/bytebuffer.h"
#include "logger.h"
#include "network.h"
#include "packet.h"
#include "utils.h"
#include "utils/bitwise.h"

#include <pthread.h>
#include <string.h>
#include <sys/socket.h>

#define MAX_PACKET_SIZE 2097151

static enum IOCode send_bytebuf(ByteBuffer* buffer, int sockfd) {
    if (buffer->size == 0)
        return IOC_OK;

    u64 sent;
    enum IOCode code = try_send(sockfd, offset(buffer->buf, buffer->read_head), buffer->size, &sent);

    bytebuf_read(buffer, sent, NULL);
    return code;
}

void write_packet(const Packet* pkt, Connection* conn) {
    pkt_encoder encoder = get_pkt_encoder(pkt, conn);

    ByteBuffer scratch = bytebuf_create_fixed(MAX_PACKET_SIZE, &conn->arena);

    bytebuf_write_varint(&scratch, pkt->id);
    encoder(pkt, &scratch);

    log_tracef("Sending: { Size: %zu, ID: %zu }", scratch.size, pkt->id);

    pthread_mutex_lock(&conn->mutex);

    bytebuf_write_varint(&conn->send_buffer, scratch.size);
    bytebuf_copy(&conn->send_buffer, &scratch);

    send_bytebuf(&conn->send_buffer, conn->sockfd);

    pthread_mutex_unlock(&conn->mutex);
}

enum IOCode sender_send(Connection* conn) {
    pthread_mutex_lock(&conn->mutex);

    enum IOCode code = send_bytebuf(&conn->send_buffer, conn->sockfd);

    pthread_mutex_unlock(&conn->mutex);
    return code;
}
