#include "containers/object_pool.h"
#include "containers/vector.h"
#include "definitions.h"
#include "logger.h"
#include "memory/arena.h"
#include "memory/mem_tags.h"
#include "registries.h"
#include "registry.h"
#include "resource/resource_id.h"
#include "utils/string.h"
#include "world/data/block.h"

static Arena arena;
static ObjectPool property_pool;

static i64 PROPERTY_LEVEL;

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
i64 register_bool_property(string name) {
    i64 index = -1;

    if (!validate_property_name(&name)) {
        log_errorf("Invalid state property name '%s'", cstr(&name));
        return -1;
    }

    StateProperty* property = objpool_add(&property_pool, &index);
    *property = (StateProperty){
        .type = BLOCK_PROP_BOOL,
        .name = name,
    };
    return index;
}
i64 register_integer_property(string name, i32 minimum, i32 maximum) {
    i64 index = -1;

    if (!validate_property_name(&name)) {
        log_errorf("Invalid state property name '%s'", cstr(&name));
        return -1;
    }

    StateProperty* property = objpool_add(&property_pool, &index);
    *property = (StateProperty){
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
i64 register_enum_state_property(string name, Arena* arena, string* values, u64 value_count) {
    i64 index = -1;

    if (!validate_property_name(&name)) {
        log_errorf("Invalid state property name '%s'", cstr(&name));
        return FALSE;
    }

    StateProperty* property = objpool_add(&property_pool, &index);
    *property = (StateProperty){
        .name = name,
        .type = BLOCK_PROP_ENUM,
        .info.enumeration =
            {
                               .value_count = value_count,
                               .values = arena_callocate(arena, sizeof *values * value_count, ALLOC_TAG_WORLD),
                               },
    };

    for (u32 i = 0; i < value_count; i++) {
        string* ptr = &property->info.enumeration.values[i];
        *ptr = values[i];
        if (!validate_property_name(ptr)) {
            log_errorf("Possible value of enumeration state property has an invalid name '%s'",
                       cstr(ptr));
            arena_free_ptr(arena, property->info.enumeration.values);
            return FALSE;
        }
    }
    return index;
}

static void init_properties(void) {

    PROPERTY_LEVEL = register_integer_property(str_view("level"), 0, 7);
}

#define REGISTER_BLOCK(blk) registry_register(block_key, blk.id, &blk)
#define REGISTER_BLOCK(blk) registry_register(block_key, blk.id, &blk)
#define CREATE_STATE_DEF(blk)                                                                      \
    create_state_definition(&blk, &properties, &arena, &blk.state_definition)
#define CREATE_SIMPLE_STATE_DEF(blk)                                                               \
    create_state_definition(&blk, NULL, &arena, &blk.state_definition)

#define SIMPLE_BLOCK(name) register_simple_block(name, &arena)

static void register_simple_block(const char* name, Arena* arena) {
    Block blk = {
        .id = resid_default_cstr(name),
        .properties = default_block_properties(),
    };

    create_state_definition(&blk, NULL, arena, &blk.state_definition);
    registry_register(BLOCK_KEY, blk.id, &blk);
}

void register_blocks(void) {

    arena = arena_create(1 << 16, BLK_TAG_REGISTRY);
    objpool_init(&property_pool, &arena, 128, sizeof(StateProperty));
    if(!registry_create(BLOCK_KEY, sizeof(Block))) {
        log_fatal("Could not create blocks registry.");
        return;
    }

    init_properties();

    Vector property_buffer;
    vect_init(&property_buffer, &arena, 16, sizeof(StateProperty*));

    SIMPLE_BLOCK("stone");
}
