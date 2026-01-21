#include "compression.h"
#include "network/common_types.h"
#include "packet.h"
#include "packet_codec.h"
#include "security.h"

#include "containers/bytebuffer.h"
#include "logger.h"
#include "platform/network.h"

#define MAX_PACKET_SIZE 2097151

void send_packet(const Packet* pkt, Connection* conn) {
    pkt_encoder encoder = get_pkt_encoder(pkt, conn);
    if (!encoder)
        return;

    mcmutex_lock(&conn->mutex);
    Arena scratch_arena       = conn->scratch_arena;
    ByteBuffer scratch_buffer = bytebuf_create_fixed(MAX_PACKET_SIZE, &scratch_arena);

    bytebuf_write_varint(&scratch_buffer, pkt->id);
    encoder(pkt, &scratch_buffer);

    log_debugf("Packet OUT: %s", get_pkt_name(pkt, conn, true));

    if (conn->compression) {
        if (scratch_buffer.size >= conn->cmprss_ctx.threshold) {
            u64 uncompressed_size         = scratch_buffer.size;
            ByteBuffer compressed_scratch = bytebuf_create_fixed(uncompressed_size, &scratch_arena);
            compression_compress(&conn->cmprss_ctx, &compressed_scratch, &scratch_buffer);

            bytebuf_prepend_varint(&compressed_scratch, uncompressed_size);
            scratch_buffer = compressed_scratch;
        } else {
            bytebuf_prepend_varint(&scratch_buffer, 0);
        }
    }

    bytebuf_prepend_varint(&scratch_buffer, scratch_buffer.size);

    if (conn->encryption) {
        if (!encryption_cipher(&conn->peer_enc_ctx, &scratch_buffer, 0))
            goto cleanup;
    }

    bytebuf_write_buffer(&conn->send_buffer, &scratch_buffer);

    if (!conn->pending_send) {
        enum IOCode code;
        do {
            code = empty_buffer(conn);
        } while (code == IOC_OK && conn->send_buffer.size > 0);
        if (code == IOC_PENDING || code == IOC_AGAIN)
            conn->pending_send = true;
    }

cleanup:

    mcmutex_unlock(&conn->mutex);
}
