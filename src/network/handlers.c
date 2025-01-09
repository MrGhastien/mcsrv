#include "handlers.h"
#include "compression.h"
#include "connection.h"
#include "containers/bytebuffer.h"
#include "network.h"
#include "packet.h"
#include "packet_codec.h"
#include "resource/resource_id.h"
#include "security.h"
#include "utils.h"

#include "containers/vector.h"
#include "data/json.h"
#include "logger.h"
#include "memory/arena.h"
#include "memory/mem_tags.h"
#include "platform/platform.h"
#include "platform/time.h"
#include "utils/string.h"

#include <string.h>
#include <zlib.h>

DEF_PKT_HANDLER(dummy) {
    UNUSED(pkt);
    UNUSED(conn);
    return TRUE;
}

DEF_PKT_HANDLER(handshake) {
    PacketHandshake* shake = pkt->payload;
    log_tracef("  - Protocol version: %i", shake->protocol_version);
    log_tracef("  - Server address: '%s'", shake->srv_addr.base);
    log_tracef("  - Port: %u", shake->srv_port);
    log_tracef("  - Next state: %i", shake->next_state);
    switch (shake->next_state) {
    case STATE_STATUS:
        log_debug("Switching connection state to STATUS.");
        conn->state = STATE_STATUS;
        break;
    case STATE_LOGIN:
        log_debug("Switching connection state to LOGIN.");
        conn->state = STATE_LOGIN;
        break;
    default:
        log_errorf("Invalid state of connection %i", shake->next_state);
        return FALSE;
    }
    return TRUE;
}

DEF_PKT_HANDLER(status) {
    UNUSED(pkt);
    PacketStatusResponse response;

    JSON json;
    Arena arena = conn->scratch_arena;
    json_create(&json, &arena);
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

    json_stringify(&json, &response.data, &arena);
    log_tracef("%s", response.data.base);
    Packet out_pkt = {.id = PKT_STATUS, .payload = &response};
    send_packet(&out_pkt, conn);
    json_destroy(&json);
    return TRUE;
}
DEF_PKT_HANDLER(ping) {
    PacketPing* ping = pkt->payload;
    PacketPing pong = {.num = ping->num};
    Packet response = {.id = PKT_STATUS_PING, .payload = &pong};

    send_packet(&response, conn);
    return TRUE;
}

DEF_PKT_HANDLER(login_start) {
    PacketLoginStart* payload = pkt->payload;

    conn->player_name = str_create_copy(&payload->player_name, &conn->persistent_arena);

    log_infof("Player '%s' is attempting to connect.", payload->player_name.base);
    log_infof("Has UUID: %016x-%016x.", payload->uuid[0], payload->uuid[1]);

    PacketCryptRequest* req = arena_callocate(&conn->scratch_arena, sizeof *req, ALLOC_TAG_PACKET);
    *req = (PacketCryptRequest){
        .server_id = str_create_view(""),
        .pkey_length = conn->global_enc_ctx->encoded_key_size,
        .pkey = conn->global_enc_ctx->encoded_key,
        .verify_tok_length = 4,
        .verify_tok = arena_allocate(&conn->scratch_arena, 4, ALLOC_TAG_UNKNOWN),
        .authenticate = TRUE,
    };
    memset(req->verify_tok, 78, req->verify_tok_length);
    conn->verify_token =
        arena_allocate(&conn->persistent_arena, req->verify_tok_length, ALLOC_TAG_UNKNOWN);
    memcpy(conn->verify_token, req->verify_tok, req->verify_tok_length);
    conn->verify_token_size = req->verify_tok_length;

    Packet response = {
        .id = PKT_LOGIN_CRYPT_REQUEST,
        .payload = req,
    };

    send_packet(&response, conn);

    return TRUE;
}
static bool enable_compression(Connection* conn) {

    PacketSetCompress payload = {
        .threshold = COMPRESS_THRESHOLD,
    };

    Packet cmprss_pkt = {
        .id = PKT_LOGIN_COMPRESS,
        .payload = &payload,
    };

    send_packet(&cmprss_pkt, conn);

    if (!compression_init(&conn->cmprss_ctx, &conn->persistent_arena))
        return FALSE;

    conn->compression = TRUE;
    log_infof("Protocol compression successfully initialized for connection %i.",
              conn->peer_socket);
    return TRUE;
}
static bool send_login_success(Connection* conn, JSON* json) {
    JSONNode* json_id = json_get_obj_cstr(json->root, "id");
    JSONNode* json_name = json_get_obj_cstr(json->root, "name");
    JSONNode* json_properties = json_get_obj_cstr(json->root, "properties");

#ifdef DEBUG

    string str;
    json_stringify(json, &str, &conn->scratch_arena);

    log_trace("Login success response: ");
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

    vect_init(
        &login_success.properties, &conn->scratch_arena, property_count, sizeof(PlayerProperty));

    for (i64 i = 0; i < property_count; i++) {
        PlayerProperty* property = vect_reserve(&login_success.properties);
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
        .id = PKT_LOGIN_SUCCESS,
        .payload = &login_success,
    };

    send_packet(&pkt, conn);

    return TRUE;
}
DEF_PKT_HANDLER(crypt_response) {
    PacketCryptResponse* payload = pkt->payload;

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

    log_infof("Protocol encryption successfully initialized for connection %i.", conn->peer_socket);

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
DEF_PKT_HANDLER(login_ack) {
    UNUSED(pkt);

    log_infof("Player %s has successfully logged-in!", str_printable_buffer(&conn->player_name));
    log_debug("Switching connection state to CONFIG.");

    conn->state = STATE_CONFIG;

    PacketCustom server_brand_payload = {
        .channel = resid_default_cstr("brand"),
        .data = bytebuf_create_fixed(32767, &conn->scratch_arena),
    };
    string srv_brand = str_create_view("mcsrv");
    bytebuf_write_varint(&server_brand_payload.data, srv_brand.length);
    bytebuf_write(&server_brand_payload.data, srv_brand.base, srv_brand.length);
    Packet pkt_to_send = {
        .id = PKT_CFG_CUSTOM_CLIENT,
        .payload = &server_brand_payload,
    };

    send_packet(&pkt_to_send, conn);

    PacketSetFeatureFlags feature_flags_payload = {0};
    vect_init(&feature_flags_payload.features, &conn->scratch_arena, 1, sizeof(ResourceID));
    ResourceID* feature = vect_reserve(&feature_flags_payload.features);
    *feature = resid_default_cstr("core");
    pkt_to_send = (Packet){
        .id = PKT_CFG_SET_FEATURE_FLAGS,
        .payload = &feature_flags_payload,
    };
    send_packet(&pkt_to_send, conn);
    
    return TRUE;
}

/* === CONFIGURATION === */

DEF_PKT_HANDLER(cfg_custom) {
    PacketCustom* payload = pkt->payload;

    if (resid_is(&payload->channel, "minecraft:brand")) {
        bytebuf_read_mcstring(&payload->data, &conn->persistent_arena, &conn->peer_brand);
        log_debugf("Peer brand: %s", str_printable_buffer(&conn->peer_brand));
    } else
        log_warnf("Received unknown custom message of channel %s:%s, ignoring.",
                  str_printable_buffer(&payload->channel.namespace),
                  str_printable_buffer(&payload->channel.path));

    return TRUE;
}

DEF_PKT_HANDLER(cfg_client_info) {
    UNUSED(conn);
    PacketClientInfo* payload = pkt->payload;

    UNUSED(payload);
    log_warn("Client settings are ignored for now TODO");
    return TRUE;
}
DEF_PKT_HANDLER(cfg_known_datapacks) {
    UNUSED(conn);
    PacketKnownDatapacks* payload = pkt->payload;
    UNUSED(payload);

    // TODO: Work with registry data
    return TRUE;
}
DEF_PKT_HANDLER(cfg_finish_config_ack) {
    UNUSED(pkt);

    log_infof("Configuration for player %s is finished.", str_printable_buffer(&conn->player_name));
    // LETS GO GAMING!!!
    conn->state = STATE_PLAY;

    return TRUE;
}
DEF_PKT_HANDLER(cfg_keep_alive) {
    UNUSED(pkt);
    UNUSED(conn);
    PacketKeepAlive* payload = pkt->payload;
    conn->last_keep_alive_id = payload->keep_alive_id;
    struct timespec now;
    if (!timestamp(&now)) {
        log_warnf("Failed to register keep alive: %s", get_last_error());
        return FALSE;
    }
    conn->last_keep_alive = now;
    return TRUE;
}
DEF_PKT_HANDLER(cfg_pong) {
    PacketPing* payload = pkt->payload;

    conn->ping = payload->num;
    return TRUE;
}
DEF_PKT_HANDLER(cfg_respack_response) {
    UNUSED(conn);
    PacketResourcePackResponse* payload = pkt->payload;
    log_debugf("Received resource pack download result: uuid: %016x-%016x -> %i",
               payload->uuid[0],
               payload->uuid[1],
               payload->result);
    // TODO: Handle resource packs
    return TRUE;
}
