#include "registry.h"
#include "containers/dict.h"
#include "memory/arena.h"
#include "resource/resource_id.h"
#include <stdlib.h>

#define REGISTRY_ARENA_SIZE 1048576

typedef struct registry {
    ResourceID name;
    Dict entries;
} Registry;

static Registry root;
static Arena arena;

void registry_system_init(void) {
    arena     = arena_create(REGISTRY_ARENA_SIZE);
    root.name = resid_default_cstr("root");
    dict_init_fixed(&root.entries, &arena, 100, sizeof(ResourceID), sizeof(Registry));
}

i64 registry_create(ResourceID name, u64 stride) {
    Registry reg = {.name = name};
    dict_init_fixed(&reg.entries, &arena, 512, sizeof(ResourceID), stride);

    return dict_put(&root.entries, &name, &reg);
}

void registry_register(i64 registry, ResourceID id, void* instance) {
    Registry* reg = dict_ref(&root.entries, registry);
    if(!reg)
        abort();

    dict_put(&reg->entries, &id, instance);
}
