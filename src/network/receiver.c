#include "connection.h"
#include "containers/bytebuffer.h"
#include "logger.h"
#include "memory/arena.h"
#include "memory/mem_tags.h"
#include "network/common_types.h"
#include "network/compression.h"
#include "packet.h"
#include "packet_codec.h"

#include <stdio.h>

typedef struct RecvContext {
    CompressionContext* compression_ctx;
    ByteBuffer* pkt_buffer;
    Arena* arena;
    bool compression_enabled;
} RecvContext;

static enum IOCode decode_common_data(RecvContext* ctx, Packet* out_pkt) {

    // Decode the packet size (ID + payload)
    i32 pkt_length;
    if (out_pkt->total_length == 0) {
        i64 length_byte_count = bytebuf_read_varint(ctx->pkt_buffer, &pkt_length);
        if (length_byte_count == 0)
            return IOC_AGAIN;
        if (length_byte_count < 0)
            return IOC_ERROR;

        // Save the total length in the packet.
        out_pkt->total_length = pkt_length;
    } else {
        // Restore the packet length from the cache.
        pkt_length = (i32) out_pkt->total_length;
    }


    if (bytebuf_size(ctx->pkt_buffer) < (u64) pkt_length)
        return IOC_AGAIN;

    out_pkt->payload_length = pkt_length;

    // TODO: Properly handle compressed packets HERE
    if (ctx->compression_enabled) {
        i32 uncompressed_length;
        i64 byte_count = bytebuf_read_varint(ctx->pkt_buffer, &uncompressed_length);
        if (byte_count < 0)
            return IOC_ERROR;

        if (uncompressed_length > 0) {

            ByteBuffer* uncompressed = arena_callocate(
                ctx->arena, sizeof *uncompressed, ALLOC_TAG_BYTEBUFFER);
            *uncompressed = bytebuf_create_fixed(uncompressed_length, ctx->arena);

            i64 decompression_result =
                compression_decompress(ctx->compression_ctx, uncompressed, ctx->pkt_buffer);
            if (decompression_result < 0 || decompression_result != uncompressed_length)
                return IOC_ERROR;

            ctx->pkt_buffer = uncompressed; // Replace the byte buffer
        }
        out_pkt->payload_length -= byte_count;
    }

    // Decode the packet ID
    i32 id;
    i64 id_size = bytebuf_read_varint(ctx->pkt_buffer, &id);
    if (id_size < 0) {
        log_error("Received invalid packet id");
        return IOC_ERROR;
    }

    // Save the payload length and the packet ID
    out_pkt->payload_length -= id_size;
    out_pkt->id = id;
    return IOC_OK;
}

static enum IOCode decode_packet(ByteBuffer* bytes, Connection* conn, Packet* out_pkt) {
    RecvContext recv_ctx = {
        .pkt_buffer = bytes,
        .arena = &conn->scratch_arena,
        .compression_ctx = &conn->cmprss_ctx,
        .compression_enabled = conn->compression,
    };

    enum IOCode ioc = decode_common_data(&recv_ctx, out_pkt);
    if (ioc != IOC_OK)
        return ioc;

    log_debugf("Packet IN: %s", get_pkt_name(out_pkt, conn, FALSE));

    pkt_decoder decoder = get_pkt_decoder(out_pkt, conn);
    if (!decoder) {
        log_error("Received packet is not supported yet !");
        return IOC_ERROR;
    }

    u64 previous_size = recv_ctx.pkt_buffer->size;
    decoder(out_pkt, &conn->scratch_arena, recv_ctx.pkt_buffer);
    u64 total_read = previous_size - recv_ctx.pkt_buffer->size;

    if (total_read != out_pkt->payload_length) {
        log_errorf("Mismatched read (%zu) vs expected (%zu) data size.",
                   total_read,
                   out_pkt->payload_length);
        out_pkt->payload = NULL;
        abort();
    }

    return IOC_OK;
}

static bool handle_packet(const Packet* pkt, Connection* conn) {
    pkt_acceptor handler = get_pkt_handler(pkt, conn);
    if (!handler)
        return FALSE;
    return handler(pkt, conn);
}

enum IOCode receive_packet(Connection* conn) {

    enum IOCode code = IOC_OK;

    // TODO: HANDLE DECOMPRESSION & DECRYPTION !

    while (code == IOC_OK) {
        if (!conn_is_resuming_read(conn)) {
            arena_save(&conn->scratch_arena);
            conn->packet_cache = arena_callocate(&conn->scratch_arena, sizeof *conn->packet_cache, ALLOC_TAG_PACKET);
            conn->packet_cache->id = PKT_INVALID;
        }

        log_trace("Trying to receive packet.");
        code = decode_packet(&conn->recv_buffer, conn, conn->packet_cache);
        log_tracef("RESULT : %i", code);
        if (code != IOC_OK)
            return code;

        if (!handle_packet(conn->packet_cache, conn))
            return IOC_ERROR;

        conn->packet_cache = NULL;
        arena_restore(&conn->scratch_arena);
    }

    return code;
}
