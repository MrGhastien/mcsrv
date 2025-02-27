#include "containers/object_pool.h"
#include "containers/vector.h"
#include "data/json.h"
#include "definitions.h"
#include "logger.h"
#include "memory/_memory_internal.h"
#include "memory/arena.h"
#include "memory/mem_tags.h"
#include "registries.h"
#include "registry.h"
#include "resource/resource_id.h"
#include "utils/str_builder.h"
#include "utils/string.h"
#include "world/data/block.h"
#include <stdlib.h>

static Arena arena;
static ObjectPool property_pool;

// static i64 PROPERTY_LEVEL;

static const ResourceID BLOCK_KEY = {
    .namespace =
        {
                    .base = "minecraft",
                    .length = 9,
                    },
    .path = {
                    .base = "blocks",
                    .length = 6,
                    }
};

static bool validate_property_name(const string* name) {
    char c;
    for (u32 i = 0; i < name->length; i++) {
        c = name->base[i];
        if (c >= 'a' && c <= 'z')
            continue;
        if (c >= 'A' && c <= 'Z')
            continue;
        if (c >= '0' && c <= '9')
            continue;
        if (c == '_')
            continue;
        return FALSE;
    }
    return TRUE;
}
i64 register_bool_state_property(string name) {
    i64 index = -1;

    if (!validate_property_name(&name)) {
        log_errorf("Invalid state property name '%s'", cstr(&name));
        return -1;
    }

    StateProperty* property = objpool_add(&property_pool, &index);
    *property = (StateProperty) {
        .type = BLOCK_PROP_BOOL,
        .name = name,
    };
    return index;
}
i64 register_integer_state_property(string name, i32 minimum, i32 maximum) {
    i64 index = -1;

    if (!validate_property_name(&name)) {
        log_errorf("Invalid state property name '%s'", cstr(&name));
        return -1;
    }

    StateProperty* property = objpool_add(&property_pool, &index);
    *property = (StateProperty) {
        .type = BLOCK_PROP_INTEGER,
        .name = name,
        .info.integer =
            {
                           .min = minimum,
                           .max = maximum,
                           },
    };
    return index;
}
i64 register_enum_state_property(string name, string* values, u64 value_count) {
    i64 index = -1;

    if (!validate_property_name(&name)) {
        log_errorf("Invalid state property name '%s'", cstr(&name));
        return FALSE;
    }

    StateProperty* property = objpool_add(&property_pool, &index);
    *property = (StateProperty) {
        .name = name,
        .type = BLOCK_PROP_ENUM,
        .info.enumeration =
            {
                               .value_count = value_count,
                               .values = values,
                               },
    };

    for (u32 i = 0; i < value_count; i++) {
        string* ptr = &property->info.enumeration.values[i];
        *ptr = values[i];
        if (!validate_property_name(ptr)) {
            log_errorf("Possible value of enumeration state property has an invalid name '%s'",
                       cstr(ptr));
            return FALSE;
        }
    }
    return index;
}

static void init_properties(void) {

    // PROPERTY_LEVEL = register_integer_state_property(str_view("level"), 0, 7);
}

#define REGISTER_BLOCK(blk) registry_register(block_key, blk.id, &blk)
#define REGISTER_BLOCK(blk) registry_register(block_key, blk.id, &blk)
#define CREATE_STATE_DEF(blk)                                                                      \
    create_state_definition(&blk, &properties, &arena, &blk.state_definition)
#define CREATE_SIMPLE_STATE_DEF(blk)                                                               \
    create_state_definition(&blk, NULL, &arena, &blk.state_definition)

#define SIMPLE_BLOCK(name) register_simple_block(name, &arena, &b_props)

static void register_simple_block(const char* name, Arena* arena, const BlockProperties* props) {
    Block blk = {
        .id = resid_default_cstr(name),
        .properties = *props,
    };

    create_state_definition(&blk, NULL, arena, &blk.state_definition);
    registry_register(BLOCK_KEY, blk.id, &blk);
}

static void register_state_properties(JSON* json) {
    json_move_to_cstr(json, "state_properties");
    json_move_to_index(json, 0);
    do {
        json_move_to_cstr(json, "type");
        string* type = json_get_string(json);
        json_move_cstr(json, "../name");
        string* name = json_get_string(json);

        log_debugf("Registry: registring block state property %s.", cstr(name));

        if (str_compare_cstr(type, "int") == 0) {
            json_move_cstr(json, "../min");
            i32 min = json_get_int(json);
            json_move_cstr(json, "../max");
            i32 max = json_get_int(json);
            register_integer_state_property(*name, min, max);
        } else if (str_compare_cstr(type, "enum") == 0) {
            json_move_cstr(json, "../values");
            i64 len = json_get_length(json);

            string* values = arena_callocate(&arena, sizeof *values * len, ALLOC_TAG_WORLD);

            i64 idx = 0;
            json_move_to_index(json, 0);
            do {
                values[idx] = *json_get_string(json);
                idx++;
            } while (json_move_to_next_sibling(json) == JSONE_OK);
            json_move_to_parent(json);

            register_enum_state_property(*name, values, len);
        } else if (str_compare_cstr(type, "bool") == 0) {
            register_bool_state_property(*name);
        } else {
            log_fatalf("Invalid block state property type: %s.", cstr(type));
            return;
        }
        json_move_to_parent(json);
    } while (json_move_to_next_sibling(json) == JSONE_OK);
}

static void register_blocks_internal(JSON* json) {

    json_move_cstr(json, "/blocks");

    json_move_to_index(json, 0);

    do {
        string* name = json_get_name(json);

        Block blk = {0};

        if (!resid_parse(name, &arena, &blk.id)) {
            log_fatalf("Invalid block name '%s'.", cstr(name));
            abort();
            return;
        }

        json_move_to_cstr(json, "state_definition");
        i64 len = json_get_length(json);

        if (len > 0) {
            Vector state_properties;
            vect_init(&state_properties, &arena, len, sizeof(StateProperty*));
            json_move_to_index(json, 0);
            do {
                i64 idx = json_get_int(json);
                StateProperty* prop = objpool_get(&property_pool, idx);
                vect_add(&state_properties, &prop);
            } while (json_move_to_next_sibling(json) == JSONE_OK);
            json_move_to_parent(json);
            create_state_definition(&blk, &state_properties, &arena, &blk.state_definition);
        } else {
            create_state_definition(&blk, NULL, &arena, &blk.state_definition);
        }

        json_move_to_parent(json);

        registry_register(BLOCK_KEY, blk.id, &blk);
    } while (json_move_to_next_sibling(json) == JSONE_OK);
}

static void print_property(void* obj, i64 idx, void* data) {
    UNUSED(data);
    Arena scratch = arena;
    StringBuilder builder = strbuild_create(&scratch);
    StateProperty* prop = obj;
    strbuild_appendf(&builder, "%lli: ", idx);
    switch (prop->type) {
    case BLOCK_PROP_BOOL:
        strbuild_appends(&builder, "boolean");
        break;
    case BLOCK_PROP_INTEGER:
        strbuild_appends(&builder, "integer");
        break;
    case BLOCK_PROP_ENUM:
        strbuild_appends(&builder, "enumeration");
        break;
    default:
        return;
    }

    strbuild_appends(&builder, " '");
    strbuild_append(&builder, &prop->name);
    strbuild_appendc(&builder, '\'');

    switch (prop->type) {
    case BLOCK_PROP_ENUM:
        strbuild_appendc(&builder, ':');
        for (u32 i = 0; i < prop->info.enumeration.value_count; i++) {
            if (i > 0)
                strbuild_appendc(&builder, ',');
            string* val = &prop->info.enumeration.values[i];
            strbuild_appends(&builder, " '");
            strbuild_append(&builder, val);
            strbuild_appendc(&builder, '\'');
        }
        break;
    case BLOCK_PROP_INTEGER:
        strbuild_appendf(&builder, ": %i -> %i", prop->info.integer.min, prop->info.integer.max);
        break;
    default:
        break;
    }

    string str = strbuild_to_string(&builder, &scratch);
    log_debug(cstr(&str));
}

static void print_state_properties(void) {
    objpool_foreach(&property_pool, &print_property, NULL);
}

void register_blocks(void) {

    arena = arena_create(1 << 25, BLK_TAG_REGISTRY);
    objpool_init(&property_pool, &arena, 128, sizeof(StateProperty));
    if (!registry_create(BLOCK_KEY, sizeof(Block))) {
        log_fatal("Could not create blocks registry.");
        return;
    }

    init_properties();

    Vector property_buffer;
    vect_init(&property_buffer, &arena, 16, sizeof(StateProperty*));
    Arena scratch = arena_create(1 << 30, BLK_TAG_REGISTRY);
    JSON json;
    enum JSONStatus status =
        json_from_file(str_view("./data/minecraft/block.json"), &scratch, &json);
    if (status != JSONE_OK) {
        log_fatal("Could not register blocks");
        return;
    }

    register_state_properties(&json);

#ifdef DEBUG
    print_state_properties();
#endif

    register_blocks_internal(&json);
    UNUSED(register_simple_block);
}

struct property_search_data {
    StateProperty* out;
    const string* name;
};

static void property_search_iterate(void* elem, i64 idx, void* user_data) {
    UNUSED(idx);
    struct property_search_data* data = user_data;
    if (data->out)
        return;
    StateProperty* prop = elem;
    if (str_compare(data->name, &prop->name) == 0)
        data->out = prop;
}

const StateProperty* get_state_property_by_name(string name) {
    struct property_search_data data = {
        .name = &name,
        .out = NULL,
    };
    objpool_foreach(&property_pool, &property_search_iterate, &data);

    return data.out;
}
