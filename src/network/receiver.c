#include "receiver.h"
#include "connection.h"
#include "containers/bytebuffer.h"
#include "logger.h"
#include "memory/arena.h"
#include "network.h"
#include "packet.h"
#include "utils.h"
#include "utils/bitwise.h"

#include <errno.h>
#include <stdio.h>

static enum IOCode decode_packet(ByteBuffer* bytes, Connection* conn, Packet* out_pkt) {
    // Read the packet size (ID + payload)
    i32 pkt_length;
    if (out_pkt->total_length == 0) {
        i64 length_byte_count = bytebuf_read_varint(bytes, &pkt_length);
        if(length_byte_count == 0)
            return IOC_AGAIN;
        if(length_byte_count < 0)
            return IOC_ERROR;

        // Save the total length in the packet.
        out_pkt->total_length = pkt_length + length_byte_count;
        out_pkt->data_length = pkt_length;
    } else {
        pkt_length = (i32)out_pkt->data_length;
    }

    // Read the packet ID
    if(out_pkt->id == PKT_INVALID) {
        i32 id;
        i64 id_size = bytebuf_read_varint(bytes, &id);
        if(id_size == 0)
            return IOC_AGAIN;
        if (id_size < 0) {
            log_error("Received invalid packet id");
            return IOC_ERROR;
        }

        // Save the payload length and the packet ID
        out_pkt->payload_length = pkt_length - id_size;
        out_pkt->id = id;
    }

    if(bytes->size < out_pkt->payload_length)
        return IOC_AGAIN;

    pkt_decoder decoder = get_pkt_decoder(out_pkt, conn);
    if (!decoder)
        return IOC_ERROR;
    decoder(out_pkt, &conn->scratch_arena, bytes);

    log_debugf("Packet IN: %s", get_pkt_name(out_pkt, conn));

    return IOC_OK;
}

static bool handle_packet(NetworkContext* ctx, const Packet* pkt, Connection* conn) {
    pkt_acceptor handler = get_pkt_handler(pkt, conn);
    if (!handler)
        return FALSE;
    return handler(ctx, pkt, conn);
}

enum IOCode receive_packet(NetworkContext* ctx, Connection* conn) {

    enum IOCode code = IOC_OK;

    //TODO: HANDLE DECOMPRESSION & DECRYPTION !

    while (code == IOC_OK) {
        if (!conn_is_resuming_read(conn)) {
            arena_save(&conn->scratch_arena);
            conn->packet_cache = arena_callocate(&conn->scratch_arena, sizeof *conn->packet_cache);
            conn->packet_cache->id = PKT_INVALID;
        }

        code = decode_packet(&conn->recv_buffer, conn, conn->packet_cache);
        if(code != IOC_OK)
            return code;

        if (!handle_packet(ctx, conn->packet_cache, conn))
            return IOC_ERROR;

        conn->packet_cache = NULL;
        arena_restore(&conn->scratch_arena);
    }

    return code;
}
