#include "registry.h"
#include "containers/dict.h"
#include "containers/vector.h"
#include "data/json.h"
#include "definitions.h"
#include "logger.h"
#include "memory/allocators/arena.h"
#include "memory/mem_tags.h"
#include "platform/platform.h"
#include "registry/codec.h"
#include "resource/resource_id.h"

#include "registries.h"
#include "utils/string.h"
#include "world/data/block.h"
#include "world/data/dimension_type.h"
#include "world/data/mob_variants.h"
#include "world/data/painting_variant.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>

#define REGISTRY_ARENA_SIZE 1048576

typedef void (*from_json)(JSON* json,
                          Arena* scratch_arena,
                          Arena* persistent_arena,
                          void* out_data);
typedef void (*to_nbt)(const void* data,
                       Arena* scratch_arena,
                       Arena* persistent_arena,
                       NBT* out_nbt);

#define TYPE_LIST(X)                                                                               \
    X("minecraft", dimension_type, DimensionType)                                                  \
    X("minecraft", damage_type, DamageType)                                                        \
    X("minecraft", painting_variant, PaintingVariant) \
    X("minecraft", wolf_variant, WolfVariant)

struct registry_type_def {
    ResourceID id;
    from_json decoder;
    to_nbt encoder;
    u64 stride;
};
static const struct registry_type_def registry_types[] = {
#define X(namespace, name, T)                                                                      \
    {                                                                                              \
        STATIC_RESID(namespace, #name),                                                            \
        &name##_from_json_generic,                                                                 \
        &name##_to_nbt_generic,                                                                    \
        sizeof(T),                                                                                 \
    },
    TYPE_LIST(X)
#undef X
};

typedef struct registry {
    ResourceID name;
    Dict entries;
    Dict tags;
} Registry;

typedef struct {
    ResourceID name;
    bool is_tag;
} TagElement;

static Registry root;
static Arena arena;

const ResourceID REGISTRY_BLOCK_KEY            = STATIC_RESID("minecraft", "block");
const ResourceID REGISTRY_BIOME_KEY            = STATIC_RESID("minecraft", "worldgen/biome");
const ResourceID REGISTRY_CHAT_TYPE_KEY        = STATIC_RESID("minecraft", "chat_type");
const ResourceID REGISTRY_TRIM_PATTERN_KEY     = STATIC_RESID("minecraft", "trim_pattern");
const ResourceID REGISTRY_TRIM_MATERIAL_KEY    = STATIC_RESID("minecraft", "trim_material");
const ResourceID REGISTRY_WOLF_VARIANT_KEY     = STATIC_RESID("minecraft", "wolf_variant");
const ResourceID REGISTRY_PAINTING_VARIANT_KEY = STATIC_RESID("minecraft", "painting_variant");
const ResourceID REGISTRY_DIMENSION_TYPE_KEY   = STATIC_RESID("minecraft", "dimension_type");
const ResourceID REGISTRY_DAMAGE_TYPE_KEY      = STATIC_RESID("minecraft", "damage_type");
const ResourceID REGISTRY_BANNER_PATTERN_KEY   = STATIC_RESID("minecraft", "banner_pattern");
const ResourceID REGISTRY_ENCHANTMENT_KEY      = STATIC_RESID("minecraft", "enchantment");
const ResourceID REGISTRY_JUKEBOX_SONG_KEY     = STATIC_RESID("minecraft", "jukebox_song");

static void initialize_registries(void) {
    // TODO: Put correct structures here !
    registry_create(REGISTRY_BLOCK_KEY, sizeof(Block));
    registry_create(REGISTRY_BIOME_KEY, sizeof(Block));
    registry_create(REGISTRY_CHAT_TYPE_KEY, sizeof(Block));
    registry_create(REGISTRY_TRIM_PATTERN_KEY, sizeof(Block));
    registry_create(REGISTRY_TRIM_MATERIAL_KEY, sizeof(Block));
    registry_create(REGISTRY_WOLF_VARIANT_KEY, sizeof(Block));
    registry_create(REGISTRY_PAINTING_VARIANT_KEY, sizeof(Block));
    registry_create(REGISTRY_DIMENSION_TYPE_KEY, sizeof(DimensionType));
    registry_create(REGISTRY_DAMAGE_TYPE_KEY, sizeof(Block));
    registry_create(REGISTRY_BANNER_PATTERN_KEY, sizeof(Block));
    registry_create(REGISTRY_ENCHANTMENT_KEY, sizeof(Block));
    registry_create(REGISTRY_JUKEBOX_SONG_KEY, sizeof(Block));
}

static const struct registry_type_def* find_def(ResourceID id) {
    for (u64 i = 0; i < sizeof(registry_types) / sizeof(registry_types[0]); ++i) {
        if (resid_is(&registry_types[i].id, &id))
            return &registry_types[i];
    }
    return nullptr;
}

static void register_registry_elements(ResourceID reg, Arena* scratch) {
    // Stop being overkill and doing shit: Just make a new arena.
    // This is MUCH simpler than using only one arena to make everything: No memory corruption !

    string dirpath = format_str(scratch, "data/%s/%s/", reg.namespace.base, reg.path.base);
    DIR* dir       = opendir(cstr(&dirpath));

    const struct registry_type_def* def = find_def(reg);
    if (!def) {
        log_errorf("Failed to register elements of " RESID_FORMAT, RESID_UNWRAP(reg));
        return;
    }

    void* obj_buf = arena_allocate(scratch, def->stride);

    struct dirent* entry;
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        string filename = str_view(entry->d_name);
        string filepath = str_concat(dirpath, filename, scratch);
        JSON json;
        if (json_from_file(filepath, scratch, &json) != JSONE_OK)
            return;

        ArenaCheckpoint checkpoint = arena_create_checkpoint(scratch);
        def->decoder(&json, scratch, &arena, obj_buf);
        arena_restore_checkpoint(checkpoint);

        i64 extension_pos = str_find_char(filename, '.');
        platform_assert(extension_pos > 0, "Invalid data file name.");

        string type_path = str_substring(filename, 0, extension_pos);
        ResourceID id    = resid_default(type_path, &arena);
        registry_register(reg, id, obj_buf);
    }
}

bool registry_entry_to_nbt(const void* entry,
                           ResourceID registry_key,
                           Arena* scratch_arena,
                           Arena* persistent_arena,
                           NBT* out_nbt) {
    const struct registry_type_def* def = find_def(registry_key);
    if (!def) {
        return false;
    }

    ArenaCheckpoint checkpoint = arena_create_checkpoint(scratch_arena);
    def->encoder(entry, scratch_arena, persistent_arena, out_nbt);
    arena_restore_checkpoint(checkpoint);

    return true;
}

static void register_data_elements(void) {
    Arena scratch = arena_create(1 << 16, BLK_TAG_REGISTRY, arena.chain);

    /*
    register_registry_elements(REGISTRY_CHAT_TYPE_KEY, &scratch);
    register_registry_elements(REGISTRY_TRIM_PATTERN_KEY, &scratch);
    register_registry_elements(REGISTRY_TRIM_MATERIAL_KEY, &scratch);
    register_registry_elements(REGISTRY_BANNER_PATTERN_KEY, &scratch);
    register_registry_elements(REGISTRY_ENCHANTMENT_KEY, &scratch);
    register_registry_elements(REGISTRY_JUKEBOX_SONG_KEY, &scratch);
    */

    register_registry_elements(REGISTRY_PAINTING_VARIANT_KEY, &scratch);
    register_registry_elements(REGISTRY_DAMAGE_TYPE_KEY, &scratch);
    register_registry_elements(REGISTRY_DIMENSION_TYPE_KEY, &scratch);
    register_registry_elements(REGISTRY_WOLF_VARIANT_KEY, &scratch);

    arena_clear(&scratch);
    arena_destroy(&scratch);
}

static void register_game_elements(void) {
    initialize_registries();
    register_blocks();
    register_data_elements();
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

static Registry* get_registry(ResourceID name) {
    i64 reg_idx;
    if ((reg_idx = dict_get(&root.entries, &name, NULL)) < 0)
        abort();

    return dict_ref(&root.entries, reg_idx);
}

bool registry_create(ResourceID name, u64 stride) {
    Registry reg = {.name = name};
    // dict_init_fixed(&reg.entries, NULL, &arena, 512, sizeof(ResourceID), stride);
    dict_init(&reg.entries, &CMP_RESID, sizeof(ResourceID), stride);
    dict_init(&reg.tags, &CMP_RESID, sizeof(ResourceID), sizeof(Vector));

    i64 idx = dict_put(&root.entries, &name, &reg);
    if (idx < 0) {
        log_errorf("Failed to create registry " RESID_FORMAT ".", RESID_UNWRAP(name));
        return false;
    }
    log_debugf("Successfully created registry " RESID_FORMAT ".", RESID_UNWRAP(name));
    return true;
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

u64 registry_count(ResourceID registry_name) {
    Registry* reg;
    i64 reg_idx;
    if ((reg_idx = dict_get(&root.entries, &registry_name, NULL)) < 0)
        abort();

    reg = dict_ref(&root.entries, reg_idx);

    return reg->entries.size;
}

i64 registry_create_tag(ResourceID registry_name, ResourceID tag_name) {
    Registry* reg = get_registry(registry_name);

    if (dict_get(&reg->tags, &tag_name, NULL) >= 0) {
        log_debugf("Tried to create already existing tag " RESID_FORMAT " in registry " RESID_FORMAT
                   ".",
                   RESID_UNWRAP(tag_name),
                   RESID_UNWRAP(tag_name));
        return -1;
    }
    Vector v;
    vect_init(&v, &arena, 16, sizeof(TagElement));

    return dict_put(&reg->tags, &tag_name, &v);
}
void registry_tag_add(ResourceID registry_name, i64 tag_idx, ResourceID object_name) {
    platform_assert(tag_idx >= 0, "Invalid tag index passed when adding objects.");

    Registry* reg        = get_registry(registry_name);
    Vector* tag_contents = dict_ref(&reg->tags, tag_idx);
    if (!tag_contents)
        platform_abort();

    TagElement elem = {.name = object_name, .is_tag = false};
    vect_add(tag_contents, &elem);
}
void registry_tag_inherit(ResourceID registry_name, i64 tag_idx, ResourceID inherit_tag_name) {

    platform_assert(tag_idx >= 0, "Invalid tag index passed when adding objects.");

    Registry* reg        = get_registry(registry_name);
    Vector* tag_contents = dict_ref(&reg->tags, tag_idx);
    if (!tag_contents)
        platform_abort();

    TagElement elem = {.name = inherit_tag_name, .is_tag = true};
    vect_add(tag_contents, &elem);
}

bool registry_is_in_tag(ResourceID registry_name, i64 tag_idx, ResourceID object_name) {
    platform_assert(tag_idx >= 0, "Invalid tag index passed when checking objects.");
    Registry* reg        = get_registry(registry_name);
    Vector* tag_contents = dict_ref(&reg->tags, tag_idx);

    for (i64 i = 0; i < vect_size(tag_contents); i++) {
        TagElement* elem_ref = vect_ref(tag_contents, i);
        if (!elem_ref->is_tag && resid_is(&elem_ref->name, &object_name))
            return true;
    }
    return false;
}

struct reg_wrapper_data {
    reg_entry_action user_action;
    void* user_data;
};

static void dict_action_wrapper(const Dict* dict, u64 idx, void* key, void* value, void* data) {
    UNUSED(dict);
    UNUSED(idx);
    struct reg_wrapper_data* wrapper_data = data;

    wrapper_data->user_action(key, value, wrapper_data->user_data);
}

void registry_foreach(ResourceID registry_name, reg_entry_action action, void* user_data) {
    Registry* reg = get_registry(registry_name);
    if (!reg)
        return;

    struct reg_wrapper_data wrapper_data = {
        .user_action = action,
        .user_data   = user_data,
    };
    dict_foreach(&reg->entries, &dict_action_wrapper, &wrapper_data);
}
