#include "registry.h"
#include "containers/dict.h"
#include "logger.h"
#include "memory/memory.h"
#include "resource/resource_id.h"

#include "registries.h"
#include "world/data/block.h"

#include <stdlib.h>

#define REGISTRY_ARENA_SIZE 1048576

typedef struct registry {
    ResourceID name;
    Dict entries;
} Registry;

static Registry root;
static Arena arena;

const ResourceID REGISTRY_BLOCK_KEY = STATIC_RESID("minecraft", "block");
const ResourceID REGISTRY_BIOME_KEY = STATIC_RESID("minecraft", "worldgen/biome");
const ResourceID REGISTRY_CHAT_TYPE_KEY = STATIC_RESID("minecraft", "chat_type");
const ResourceID REGISTRY_TRIM_PATTERN_KEY = STATIC_RESID("minecraft", "trim_pattern");
const ResourceID REGISTRY_TRIM_MATERIAL_KEY = STATIC_RESID("minecraft", "trim_material");
const ResourceID REGISTRY_WOLF_VARIANT_KEY = STATIC_RESID("minecraft", "wolf_variant");
const ResourceID REGISTRY_PAINTING_VARIANT_KEY = STATIC_RESID("minecraft", "painting_variant");
const ResourceID REGISTRY_DIMENSION_TYPE_KEY = STATIC_RESID("minecraft", "dimension_type");
const ResourceID REGISTRY_DAMAGE_TYPE_KEY = STATIC_RESID("minecraft", "damage_type");
const ResourceID REGISTRY_BANNER_PATTERN_KEY = STATIC_RESID("minecraft", "banner_pattern");
const ResourceID REGISTRY_ENCHANTMENT_KEY = STATIC_RESID("minecraft", "enchantment");
const ResourceID REGISTRY_JUKEBOX_SONG_KEY = STATIC_RESID("minecraft", "jukebox_song");

static void initialize_registries(void) {
    // TODO: Put correct structures here !
    registry_create(REGISTRY_BLOCK_KEY, sizeof(Block));
    registry_create(REGISTRY_BIOME_KEY, sizeof(Block));
    registry_create(REGISTRY_CHAT_TYPE_KEY, sizeof(Block));
    registry_create(REGISTRY_TRIM_PATTERN_KEY, sizeof(Block));
    registry_create(REGISTRY_TRIM_MATERIAL_KEY, sizeof(Block));
    registry_create(REGISTRY_WOLF_VARIANT_KEY, sizeof(Block));
    registry_create(REGISTRY_PAINTING_VARIANT_KEY, sizeof(Block));
    registry_create(REGISTRY_DIMENSION_TYPE_KEY, sizeof(Block));
    registry_create(REGISTRY_DAMAGE_TYPE_KEY, sizeof(Block));
    registry_create(REGISTRY_BANNER_PATTERN_KEY, sizeof(Block));
    registry_create(REGISTRY_ENCHANTMENT_KEY, sizeof(Block));
    registry_create(REGISTRY_JUKEBOX_SONG_KEY, sizeof(Block));
}

static void register_game_elements(void) {
    initialize_registries();
    register_blocks();
}

void registry_system_init(void) {
    arena     = arena_create(REGISTRY_ARENA_SIZE, BLK_TAG_REGISTRY, INVALID_CHAIN);
    root.name = resid_default_cstr("root");
    dict_init_fixed(&root.entries, &CMP_RESID, &arena, 64, sizeof(ResourceID), sizeof(Registry));

    log_debug("Registry subsystem initialized.");

    register_game_elements();
}

void registry_system_cleanup(void) {
    arena_destroy(&arena);
}

bool registry_create(ResourceID name, u64 stride) {
    Registry reg = {.name = name};
    // dict_init_fixed(&reg.entries, NULL, &arena, 512, sizeof(ResourceID), stride);
    dict_init(&reg.entries, &CMP_RESID, sizeof(ResourceID), stride);

    i64 idx = dict_put(&root.entries, &name, &reg);
    if (idx < 0) {
        log_errorf("Failed to create registry " RESID_FORMAT ".", RESID_UNWRAP(name));
        return FALSE;
    }
    log_debugf("Successfully created registry " RESID_FORMAT ".", RESID_UNWRAP(name));
    return TRUE;
}

void registry_register(ResourceID registry_name, ResourceID id, void* instance) {
    Registry* reg;
    i64 reg_idx;
    if ((reg_idx = dict_get(&root.entries, &registry_name, NULL)) < 0)
        abort();

    reg = dict_ref(&root.entries, reg_idx);

    log_debugf("Registering " RESID_FORMAT " into registry " RESID_FORMAT ".",
               RESID_UNWRAP(id),
               RESID_UNWRAP(registry_name));
    if (dict_put(&reg->entries, &id, instance) < 0)
        log_errorf("Registration of " RESID_FORMAT " into registry " RESID_FORMAT " failed.",
                   RESID_UNWRAP(id),
                   RESID_UNWRAP(registry_name));
}

const void* registry_get(ResourceID registry_name, ResourceID element_id) {
    Registry* reg;
    i64 reg_idx;
    if ((reg_idx = dict_get(&root.entries, &registry_name, NULL)) < 0)
        abort();

    reg = dict_ref(&root.entries, reg_idx);

    i64 idx = dict_get(&reg->entries, &element_id, NULL);
    if (idx == -1) {
        log_errorf("Failed to get element " RESID_FORMAT " of registry " RESID_FORMAT ".",
                   RESID_UNWRAP(element_id),
                   RESID_UNWRAP(registry_name));
        return NULL;
    }

    return dict_ref(&reg->entries, idx);
}
