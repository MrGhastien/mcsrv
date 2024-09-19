#include "sender.h"
#include "containers/bytebuffer.h"
#include "logger.h"
#include "memory/arena.h"
#include "network.h"
#include "network/compression.h"
#include "network/encryption.h"
#include "packet.h"
#include "utils.h"
#include "utils/bitwise.h"
#include "utils/math.h"

#include <pthread.h>
#include <string.h>
#include <sys/socket.h>

#define MAX_PACKET_SIZE 2097151

static enum IOCode send_bytebuf(ByteBuffer* buffer, int sockfd) {
    if (buffer->size == 0) {
        log_warn("No data to send.");
        return IOC_OK;
    }

    u64 total_sent = 0;
    u64 sent;

    u64 to_send = min_u64(buffer->capacity - buffer->read_head, buffer->size);
    enum IOCode code = try_send(sockfd, offset(buffer->buf, buffer->read_head), to_send, &sent);
    total_sent += sent;

    if (to_send < buffer->size && code == IOC_OK) {
        code = try_send(sockfd, buffer->buf, buffer->size - to_send, &sent);
        total_sent += sent;
    }

    bytebuf_read(buffer, total_sent, NULL);
    return code;
}

static bool encrypt_bytebuf(ByteBuffer* buffer, PeerEncryptionContext* enc_ctx) {
    u64 to_cipher = min_u64(buffer->capacity - buffer->read_head, buffer->size);

    if (encryption_cipher(enc_ctx, offset(buffer->buf, buffer->read_head), to_cipher) == -1)
        return FALSE;

    if (to_cipher < buffer->size) {
        if (encryption_cipher(enc_ctx, buffer->buf, buffer->size - to_cipher) == -1)
            return FALSE;
    }

    return TRUE;
}

void write_packet(const Packet* pkt, Connection* conn) {
    pkt_encoder encoder = get_pkt_encoder(pkt, conn);
    if (!encoder)
        return;

    arena_save(&conn->scratch_arena);
    ByteBuffer scratch = bytebuf_create_fixed(MAX_PACKET_SIZE, &conn->scratch_arena);

    bytebuf_write_varint(&scratch, pkt->id);
    encoder(pkt, &scratch);

    log_debugf("Packet OUT: %s", get_pkt_name(pkt, conn));

    if (conn->compression) {
        if (scratch.size >= COMPRESS_THRESHOLD) {
            u64 uncompressed_size = scratch.size;
            ByteBuffer compressed_scratch =
                bytebuf_create_fixed(uncompressed_size, &conn->scratch_arena);
            compression_compress(&conn->cmprss_ctx, &compressed_scratch, &scratch);

            bytebuf_prepend_varint(&compressed_scratch, uncompressed_size);
            scratch = compressed_scratch;
        } else {
            bytebuf_prepend_varint(&scratch, 0);
        }
    }

    bytebuf_prepend_varint(&scratch, scratch.size);

    if (conn->encryption) {
        if(!encrypt_bytebuf(&scratch, &conn->peer_enc_ctx))
            return;
    }

    pthread_mutex_lock(&conn->mutex);
    bytebuf_write_buffer(&conn->send_buffer, &scratch);

    //send_bytebuf(&conn->send_buffer, conn->sockfd);

    arena_restore(&conn->scratch_arena);
    pthread_mutex_unlock(&conn->mutex);
}

enum IOCode sender_send(Connection* conn) {
    pthread_mutex_lock(&conn->mutex);

    enum IOCode code = send_bytebuf(&conn->send_buffer, conn->sockfd);

    pthread_mutex_unlock(&conn->mutex);
    return code;
}
