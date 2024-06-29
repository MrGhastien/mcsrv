#include "json/json.h"
#include "logger.h"
#include "network/connection.h"
#include "packet.h"
#include "sender.h"

bool pkt_handle_dummy(const Packet* pkt, Connection* conn) {
    (void)pkt;
    (void)conn;
    return TRUE;
}

bool pkt_handle_handshake(Packet* pkt, Connection* conn) {
    PacketHandshake* shake = pkt->payload;
    log_debugf("  - Protocol version: %i", shake->protocol_version);
    log_debugf("  - Server address: '%s'", shake->srv_addr.base);
    log_debugf("  - Port: %u", shake->srv_port);
    log_debugf("  - Next state: %i", shake->next_state);
    switch(shake->next_state) {
    case STATE_STATUS:
    case STATE_LOGIN:
        conn->state = shake->next_state;
        break;
    default:
        log_errorf("Invalid state of connection %i", shake->next_state);
        return FALSE;
    }
    return TRUE;
}

bool pkt_handle_status(Packet* pkt, Connection* conn) {
    (void)pkt;
    (void)conn;
    PacketStatusResponse response;

    JSON json;
    arena_save(&conn->arena);
    json_create(&json, &conn->arena);

    JSONNode* nodes[4];

    nodes[0] = json_node_put(&json, json.root, "version", JSON_OBJECT);

    nodes[1] = json_node_put(&json, nodes[0], "name", JSON_STRING);

    json_set_cstr(nodes[1], "1.21");

    nodes[1] = json_node_put(&json, nodes[0], "protocol", JSON_INT);
    json_set_int(nodes[1], 767);

    nodes[0] = json_node_put(&json, json.root, "players", JSON_OBJECT);

    nodes[1] = json_node_put(&json, nodes[0], "max", JSON_INT);
    json_set_int(nodes[1], 69);

    nodes[1] = json_node_put(&json, nodes[0], "online", JSON_INT);
    json_set_int(nodes[1], 0);

    // nodes[1] = json_node_put(&json, nodes[0], "sample", JSON_ARRAY);
    /* nodes[2] = json_node_add(&json, nodes[1], JSON_OBJECT); */
    /* nodes[3] = json_node_put(&json, nodes[2], "name", JSON_STRING); */
    /* json_set_cstr(nodes[3], "EPIC_GAMR"); */
    /* nodes[3] = json_node_put(&json, nodes[2], "id", JSON_STRING); */
    /* json_set_cstr(nodes[3], "4566e69f-c907-48ee-8d71-d7ba5aa00d20"); // Random UUID */

    nodes[0] = json_node_put(&json, json.root, "description", JSON_OBJECT);
    nodes[1] = json_node_put(&json, nodes[0], "text", JSON_STRING);
    json_set_cstr(nodes[1], "Hello gamerz!");

    nodes[0] = json_node_put(&json, json.root, "enforcesSecureChat", JSON_BOOL);
    json_set_bool(nodes[0], FALSE);

    nodes[0] = json_node_put(&json, json.root, "previewsChat", JSON_BOOL);
    json_set_bool(nodes[0], FALSE);

    json_stringify(&json, &response.data, &conn->arena);
    log_debugf("%s", response.data.base);
    Packet out_pkt = {.id = PKT_STATUS, .payload = &response};
    write_packet(&out_pkt, conn);
    return TRUE;
}

bool pkt_handle_ping(Packet* pkt, Connection* conn) {
    (void)pkt;
    (void)conn;
    PacketPing* ping = pkt->payload;
    PacketPing pong = {.num = ping->num};
    Packet response = {.id = PKT_PING, .payload = &pong};

    write_packet(&response, conn);
    return TRUE;
}


bool pkt_handle_log_start(const Packet* pkt, Connection* conn) {
    (void)conn;
    PacketLoginStart* payload = pkt->payload;

    log_infof("Player '%s' is attempting to connect.", payload->player_name.base);
    log_infof("Has UUID: %016x-%016x.", payload->uuid[0], payload->uuid[1]);

    return TRUE;
}
