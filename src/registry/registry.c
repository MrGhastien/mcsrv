#include "registry.h"
#include "containers/dict.h"
#include "logger.h"
#include "memory/memory.h"
#include "resource/resource_id.h"

#include "registries.h"

#include <stdlib.h>

#define REGISTRY_ARENA_SIZE 1048576

typedef struct registry {
    ResourceID name;
    Dict entries;
} Registry;

static Registry root;
static Arena arena;

static void register_game_elements(void) {
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

    return dict_put(&root.entries, &name, &reg) >= 0;
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
