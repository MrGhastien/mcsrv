#include "sender.h"
#include "containers/bytebuffer.h"
#include "logger.h"
#include "memory/arena.h"
#include "network.h"
#include "network/compression.h"
#include "network/security.h"
#include "packet.h"
#include "utils.h"
#include "utils/bitwise.h"
#include "utils/math.h"
#include "platform/network.h"

#include <pthread.h>

#define MAX_PACKET_SIZE 2097151

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

void write_packet(NetworkContext* ctx, const Packet* pkt, Connection* conn) {
    pkt_encoder encoder = get_pkt_encoder(pkt, conn);
    if (!encoder)
        return;

    mcmutex_lock(&conn->mutex);
    arena_save(&conn->scratch_arena);
    ByteBuffer scratch = bytebuf_create_fixed(MAX_PACKET_SIZE, &conn->scratch_arena);

    bytebuf_write_varint(&scratch, pkt->id);
    encoder(pkt, &scratch);

    log_debugf("Packet OUT: %s", get_pkt_name(pkt, conn));

    if (conn->compression) {
        if (scratch.size >= conn->cmprss_ctx.threshold) {
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
        if (!encrypt_bytebuf(&scratch, &conn->peer_enc_ctx))
            return;
    }

    bytebuf_write_buffer(&conn->send_buffer, &scratch);

    if(!conn->pending_write)
        empty_buffer(ctx, conn);


    arena_restore(&conn->scratch_arena);
    mcmutex_unlock(&conn->mutex);
}