#include "context.h"
#include "packet.h"

static pkt_acceptor handler_table[] = {};
static pkt_decoder decoder_table[] = {[PKT_HANDSHAKE] = &packet_decode_handshake};

pkt_acceptor get_pkt_handler(Connection* conn) {
}

pkt_decoder get_pkt_decoder(Packet* pkt, const Connection* conn) {
    return decoder_table[pkt->id];
}
