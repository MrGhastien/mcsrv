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
        .info.integer = {
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
        .info.enumeration = {
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

void register_blocks(void) {

    arena = arena_create(1 << 16, BLK_TAG_REGISTRY);
    objpool_init(&property_pool, &arena, 128, sizeof(StateProperty));

    ResourceID block_key = resid_default_cstr("block");
    registry_create(block_key, sizeof(Block));

    init_properties();

    Block blk = {
        .id = resid_default_cstr("stone"),
        .properties = default_block_properties(),
    };
    Vector properties;
    vect_init(&properties, &arena, 1, sizeof(StateProperty*));
    vect_add_imm(&properties, objpool_get(&property_pool, PROPERTY_LEVEL), StateProperty*);
    create_state_definition(&blk, &properties, &arena, &blk.state_definition);
    registry_register(block_key, resid_default_cstr("stone"), &blk);
}
