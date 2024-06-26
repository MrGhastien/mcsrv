#include "encoders.h"
#include "packet.h"

static void encode_string(const string* str, ByteBuffer* buffer) {
    bytebuf_write_varint(buffer, str->length);

    bytebuf_write(buffer, str->base, str->length);
}

void pkt_encode_status(const Packet* pkt, ByteBuffer* buffer) {
    PacketStatusResponse* payload = pkt->payload;
    encode_string(&payload->data, buffer);
}

void pkt_encode_ping(const Packet* pkt, ByteBuffer* buffer) {
    PacketPing* pong = pkt->payload;

    bytebuf_write_i64(buffer, pong->num);
}
