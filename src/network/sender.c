#include "compression.h"
#include "network/common_types.h"
#include "security.h"
#include "packet.h"
#include "packet_codec.h"

#include "containers/bytebuffer.h"
#include "logger.h"
#include "memory/arena.h"
#include "utils/bitwise.h"
#include "utils/math.h"
#include "platform/network.h"

#define MAX_PACKET_SIZE 2097151

void send_packet(NetworkContext* ctx, const Packet* pkt, Connection* conn) {
    pkt_encoder encoder = get_pkt_encoder(pkt, conn);
    if (!encoder)
        return;

    mcmutex_lock(&conn->mutex);
    arena_save(&conn->scratch_arena);
    ByteBuffer scratch = bytebuf_create_fixed(MAX_PACKET_SIZE, &conn->scratch_arena);

    bytebuf_write_varint(&scratch, pkt->id);
    encoder(pkt, &scratch);

    log_debugf("Packet OUT: %s", get_pkt_name(pkt, conn, TRUE));

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
        if (!encryption_cipher(&conn->peer_enc_ctx, &scratch, 0))
            return;
    }

    bytebuf_write_buffer(&conn->send_buffer, &scratch);

    if(!conn->pending_send) {
        enum IOCode code;
        do {
            code = empty_buffer(ctx, conn);
        } while(code == IOC_OK && conn->send_buffer.size > 0);
        if(code == IOC_PENDING || code == IOC_AGAIN)
            conn->pending_send = TRUE;
    }


    arena_restore(&conn->scratch_arena);
    mcmutex_unlock(&conn->mutex);
}
