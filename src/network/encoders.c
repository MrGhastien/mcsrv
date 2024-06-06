#include "encoders.h"
#include "../json/json.h"
#include "utils.h"
#include <string.h>

static void encode_varint(int n, Arena* arena) {
    while (TRUE) {
        u8* byte = arena_allocate(arena, sizeof *byte);
        *byte = n & SEGMENT_BITS;
        if (n & ~SEGMENT_BITS)
            *byte |= CONTINUE_BIT;

        n >>= 7;
    }
}

static void encode_string(char* str, Arena* arena) {
    size_t len = strlen(str);
    encode_varint(len, arena);

    char c;
    while ((c = *str)) {
        char* chr = arena_allocate(arena, sizeof *chr);
        *chr = c;
    }
}

void pkt_encode_status(const Packet* pkt, Connection* conn) {
    Arena* arena = &conn->arena;
    JSON json;
    json_create(&json, arena);

    JSONNode* nodes[4];

    nodes[0] = json_node_put(&json, json.root, "version", JSON_OBJECT);

    nodes[1] = json_node_put(&json, nodes[0], "name", JSON_STRING);

    json_set_cstr(&json, nodes[1], "1.20.6");

    nodes[1] = json_node_put(&json, nodes[0], "protocol", JSON_INT);
    json_set_int(&json, nodes[1], 765);

    nodes[0] = json_node_put(&json, json.root, "players", JSON_OBJECT);

    nodes[1] = json_node_put(&json, nodes[0], "max", JSON_INT);
    json_set_int(&json, nodes[1], 69);

    nodes[1] = json_node_put(&json, nodes[0], "online", JSON_INT);
    json_set_int(&json, nodes[1], 1);

    nodes[1] = json_node_put(&json, nodes[0], "sample", JSON_ARRAY);
    nodes[2] = json_node_add(&json, nodes[1], JSON_OBJECT);
    nodes[3] = json_node_put(&json, nodes[2], "name", JSON_STRING);
    json_set_cstr(&json, nodes[3], "EPIC_GAMR");
    nodes[3] = json_node_put(&json, nodes[2], "id", JSON_STRING);
    json_set_cstr(&json, nodes[3], "4566e69f-c907-48ee-8d71-d7ba5aa00d20"); // Random UUID

    nodes[0] = json_node_put(&json, json.root, "description", JSON_OBJECT);
    nodes[1] = json_node_put(&json, nodes[1], "text", JSON_STRING);
    json_set_cstr(&json, nodes[1], "Hello gamerz!");

    nodes[0] = json_node_put(&json, json.root, "enforcesSecureChat", JSON_BOOL);
    json_set_bool(&json, nodes[0], FALSE);

    nodes[0] = json_node_put(&json, json.root, "previewsChat", JSON_BOOL);
    json_set_bool(&json, nodes[0], FALSE);
}
