#include "encoders.h"
#include "containers/bytebuffer.h"
#include "packet.h"

static void encode_string(const string* str, ByteBuffer* buffer) {
    bytebuf_write_varint(buffer, str->length);

    bytebuf_write(buffer, str->base, str->length);
}

PKT_ENCODER(dummy) {
    (void) pkt;
    (void) buffer;
}

PKT_ENCODER(status) {
    PacketStatusResponse* payload = pkt->payload;
    encode_string(&payload->data, buffer);
}

PKT_ENCODER(ping) {
    PacketPing* pong = pkt->payload;

    bytebuf_write_i64(buffer, pong->num);
}

PKT_ENCODER(enc_req) {
    PacketEncReq* payload = pkt->payload;

    encode_string(&payload->server_id, buffer);
    bytebuf_write_varint(buffer, payload->pkey_length);
    bytebuf_write(buffer, payload->pkey, payload->pkey_length);
    bytebuf_write_varint(buffer, payload->verify_tok_length);
    bytebuf_write(buffer, payload->verify_tok, payload->verify_tok_length);
    bytebuf_write(buffer, &payload->authenticate, 1);
}

PKT_ENCODER(compress) {
    PacketSetCompress* payload = pkt->payload;

    bytebuf_write_varint(buffer, payload->threadhold);
}
