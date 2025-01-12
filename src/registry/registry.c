#include "registry.h"
#include "containers/dict.h"
#include "logger.h"
#include "memory/arena.h"
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
    arena = arena_create(REGISTRY_ARENA_SIZE, BLK_TAG_REGISTRY);
    root.name = resid_default_cstr("root");
    dict_init_fixed(&root.entries, NULL, &arena, 100, sizeof(ResourceID), sizeof(Registry));

    log_debug("Registry subsystem initialized.");

    register_game_elements();
}

void registry_system_cleanup(void) {
    arena_destroy(&arena);
}

bool registry_create(ResourceID name, u64 stride) {
    Registry reg = {.name = name};
    dict_init_fixed(&reg.entries, NULL, &arena, 512, sizeof(ResourceID), stride);

    return dict_put(&root.entries, &name, &reg);
}

void registry_register(ResourceID registry_name, ResourceID id, void* instance) {
    Registry reg;
    if (dict_get(&root.entries, &registry_name, &reg) < 0)
        abort();

    dict_put(&reg.entries, &id, instance);
}
