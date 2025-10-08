#include "entity.h"
#include "definitions.h"
#include "player.h"

#include "memory/_memory_internal.h"
#include "memory/allocators/pool.h"
#include "memory/mem_tags.h"

#include <string.h>

static u64 component_sizes[] = {
    [EC_POSITION] = sizeof(TransformComponent),
    [EC_PLAYER]   = sizeof(PlayerComponent),
};

static PoolAllocator component_pools[_EC_COUNT];
static PoolAllocator things;

void ecomponents_init(void) {

    pool_init(&things, 512, sizeof(Entity), BLK_TAG_ENTITY, INVALID_CHAIN);

    memory_chain prev_chain = things.mem;
    for (int i = 0; i < _EC_COUNT; i++) {
        pool_init(&component_pools[i], 512, component_sizes[i], BLK_TAG_ENTITY, prev_chain);
        prev_chain = component_pools[i].mem;
    }
}

void ecomponents_cleanup(void) {
    // All chains from all pools are linked together
    pool_destroy(&things);
}

long create_thing(void) {
    long idx;
    pool_alloc(&things, &idx);
    return idx;
}

void ecomponents_attach(long entity_id, enum EntityComponentType type, const void* data) {
    Entity* t = pool_get(&things, entity_id);

    long component_idx;
    void* component = pool_alloc(&component_pools[type], &component_idx);

    memcpy(component, data, component_sizes[type]);

    /* WARNING */
    // THE FIRST MEMBER OF COMPONENT STRUCTS MUST ALWAYS BE THE ENTITY ID !
    *(long*) component = entity_id;

    t->components[type] = component_idx;
}

void* ecomponents_get(enum EntityComponentType type, long entity_id) {
    Entity* t = pool_get(&things, entity_id);
    return pool_get(&component_pools[type], t->components[type]);
}

void ecomponents_tick(double delta_time) {
    UNUSED(delta_time);
}
