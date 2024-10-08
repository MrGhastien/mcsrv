#include "handlers.h"

#include "containers/vector.h"
#include "logger.h"
#include "memory/arena.h"
#include "network.h"
#include "network/compression.h"
#include "network/connection.h"
#include "network/security.h"
#include "network/utils.h"
#include "packet.h"
#include "sender.h"
#include "utils/string.h"
#include "json/json.h"

#include "json/json_internal.h"
#include <string.h>
#include <zlib.h>

PKT_HANDLER(dummy) {
    (void) pkt;
    (void) conn;
    return TRUE;
}

PKT_HANDLER(handshake) {
    PacketHandshake* shake = pkt->payload;
    log_tracef("  - Protocol version: %i", shake->protocol_version);
    log_tracef("  - Server address: '%s'", shake->srv_addr.base);
    log_tracef("  - Port: %u", shake->srv_port);
    log_tracef("  - Next state: %i", shake->next_state);
    switch (shake->next_state) {
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

PKT_HANDLER(status) {
    (void) pkt;
    (void) conn;
    PacketStatusResponse response;

    JSON json;
    arena_save(&conn->scratch_arena);
    json_create(&json, &conn->scratch_arena);
    json_set_root(&json, json_node_create(&json, JSON_OBJECT));

    JSONNode* nodes[4];

    nodes[0] = json_node_put(&json, json.root, "version", JSON_OBJECT);

    nodes[1] = json_node_put(&json, nodes[0], "name", JSON_STRING);

    json_set_cstr(&json, nodes[1], "1.21");

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
    json_set_cstr(&json, nodes[1], "Hello gamerz!");

    nodes[0] = json_node_put(&json, json.root, "enforcesSecureChat", JSON_BOOL);
    json_set_bool(nodes[0], FALSE);

    nodes[0] = json_node_put(&json, json.root, "previewsChat", JSON_BOOL);
    json_set_bool(nodes[0], FALSE);

    json_stringify(&json, &response.data, 2048, &conn->scratch_arena);
    log_tracef("%s", response.data.base);
    Packet out_pkt = {.id = PKT_STATUS, .payload = &response};
    write_packet(&out_pkt, conn);
    json_destroy(&json);
    return TRUE;
}

PKT_HANDLER(ping) {
    (void) pkt;
    (void) conn;
    PacketPing* ping = pkt->payload;
    PacketPing pong = {.num = ping->num};
    Packet response = {.id = PKT_PING, .payload = &pong};

    write_packet(&response, conn);
    return TRUE;
}

PKT_HANDLER(log_start) {
    (void) conn;
    PacketLoginStart* payload = pkt->payload;

    conn->player_name = str_create_copy(&payload->player_name, &conn->persistent_arena);

    log_infof("Player '%s' is attempting to connect.", payload->player_name.base);
    log_infof("Has UUID: %016x-%016x.", payload->uuid[0], payload->uuid[1]);

    PacketEncReq* req = arena_callocate(&conn->scratch_arena, sizeof *req);
    *req = (PacketEncReq){
        .server_id = str_create_const(""),
        .pkey_length = conn->global_enc_ctx->encoded_key_size,
        .pkey = conn->global_enc_ctx->encoded_key,
        .verify_tok_length = 4,
        .verify_tok = arena_allocate(&conn->scratch_arena, 4),
        .authenticate = TRUE,
    };
    memset(req->verify_tok, 78, req->verify_tok_length);
    conn->verify_token = arena_allocate(&conn->persistent_arena, req->verify_tok_length);
    memcpy(conn->verify_token, req->verify_tok, req->verify_tok_length);
    conn->verify_token_size = req->verify_tok_length;

    Packet response = {
        .id = PKT_ENC_REQ,
        .payload = req,
    };

    write_packet(&response, conn);

    return TRUE;
}

static bool enable_compression(Connection* conn) {

    PacketSetCompress payload = {
        .threshold = COMPRESS_THRESHOLD,
    };

    Packet cmprss_pkt = {
        .id = PKT_COMPRESS,
        .payload = &payload,
    };

    write_packet(&cmprss_pkt, conn);

    if (!compression_init(&conn->cmprss_ctx, &conn->persistent_arena))
        return FALSE;

    conn->compression = TRUE;
    log_infof("Protocol compression successfully initialized for connection %i.", conn->sockfd);
    return TRUE;
}

static bool send_login_success(Connection* conn, JSON* json) {
    JSONNode* json_id = json_get_obj_cstr(json->root, "id");
    JSONNode* json_name = json_get_obj_cstr(json->root, "name");
    JSONNode* json_properties = json_get_obj_cstr(json->root, "properties");

#ifdef DEBUG

    string str;
    json_stringify(json, &str, 1 << 12, &conn->scratch_arena);

    log_trace(str.base);

#endif

    if (!json_id)
        return FALSE;
    if (!json_name)
        return FALSE;
    if (!json_properties)
        return FALSE;

    string* name = json_get_str(json_name);
    if (!name)
        return FALSE;

    PacketLoginSuccess login_success = {
        .username = str_create_copy(name, &conn->scratch_arena),
        .strict_errors = TRUE,
    };
    string* uuid = json_get_str(json_id);
    if (!parse_uuid(uuid, login_success.uuid))
        return FALSE;

    i64 property_count = json_get_length(json_properties);
    if (property_count == -1)
        return FALSE;

    vector_init_fixed(
        &login_success.properties, &conn->scratch_arena, property_count, sizeof(PlayerProperty));

    for (i64 i = 0; i < property_count; i++) {
        PlayerProperty* property = vector_reserve(&login_success.properties);
        JSONNode* json_property = json_get_array(json_properties, i);
        if (!json_property)
            return FALSE;

        JSONNode* json_prop_name = json_get_obj_cstr(json_property, "name");
        string* prop_name = json_get_str(json_prop_name);
        property->name = str_create_copy(prop_name, &conn->scratch_arena);

        JSONNode* json_prop_value = json_get_obj_cstr(json_property, "value");
        string* prop_value = json_get_str(json_prop_value);
        property->value = str_create_copy(prop_value, &conn->scratch_arena);

        JSONNode* json_prop_sig = json_get_obj_cstr(json_property, "signature");
        if (json_prop_sig) {
            string* prop_sig = json_get_str(json_prop_sig);
            property->signature = *prop_sig;
            property->is_signed = TRUE;
        } else
            property->is_signed = FALSE;
    }

    Packet pkt = {
        .id = PKT_LOG_SUCCESS,
        .payload = &login_success,
    };

    write_packet(&pkt, conn);

    return TRUE;
}

PKT_HANDLER(enc_res) {
    PacketEncRes* payload = pkt->payload;

    u64 ss_size;
    u8* decrypted_ss = encryption_decrypt(conn->global_enc_ctx,
                                          &conn->scratch_arena,
                                          &ss_size,
                                          payload->shared_secret,
                                          payload->shared_secret_length);

    u64 verif_size;
    u8* verif_tok = encryption_decrypt(conn->global_enc_ctx,
                                       &conn->scratch_arena,
                                       &verif_size,
                                       payload->verify_token,
                                       payload->verify_token_length);

    if (!decrypted_ss || !verif_tok)
        return FALSE;

    if (verif_size != conn->verify_token_size) {
        log_error("Failed encryption: the received verify token has a different length.");
        return FALSE;
    }

    if (memcmp(verif_tok, conn->verify_token, conn->verify_token_size) != 0) {
        log_error("Failed encryption: the received & stored verify tokens differ.");
        log_trace("RE - ST");
        for (u32 i = 0; i < conn->verify_token_size; i++) {
            log_debugf("%hhx - %hhx", verif_tok[i], conn->verify_token[i]);
        }
        return FALSE;
    }

    arena_free_ptr(&conn->persistent_arena, conn->verify_token);
    if (!encryption_init_peer(&conn->peer_enc_ctx, &conn->persistent_arena, decrypted_ss))
        return FALSE;

    conn->encryption = TRUE;

    log_infof("Protocol encryption successfully initialized for connection %i.", conn->sockfd);

    JSON json;
    bool res = encryption_authenticate_player(conn, &json);
    if (!res)
        return FALSE;

    res = enable_compression(conn);
    if (!res)
        return FALSE;

    res = send_login_success(conn, &json);

    json_destroy(&json);

    return res;
}
