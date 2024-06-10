#include "connection.h"
#include "packet.h"
#include "decoders.h"
#include "handlers.h"

static pkt_acceptor handler_table[][_STATE_COUNT] = {
    [STATE_HANDSHAKE] = {[PKT_HANDSHAKE] = &pkt_handle_handshake},
    [STATE_STATUS] = {[PKT_STATUS] = &pkt_handle_status, [PKT_PING] = NULL}
};

static pkt_decoder decoder_table[][_STATE_COUNT] = {
    [STATE_HANDSHAKE] = {[PKT_HANDSHAKE] = &pkt_decode_handshake},
    [STATE_STATUS] = {[PKT_STATUS] = NULL, [PKT_PING] = NULL}
};

pkt_acceptor get_pkt_handler(Packet* pkt, Connection* conn) {
    return handler_table[conn->state][pkt->id];
}

pkt_decoder get_pkt_decoder(Packet* pkt, Connection* conn) {
    return decoder_table[conn->state][pkt->id];
}
