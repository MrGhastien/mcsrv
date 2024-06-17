#ifndef REGISTRY_H
#define REGISTRY_H

#include "definitions.h"
#include "resource/resource_id.h"

void registry_system_init(void);
void registry_system_cleanup(void);

i64 registry_create(ResourceID name, u64 stride);

void registry_register(i64 registry, ResourceID id, void* instance);

#endif /* ! REGISTRY_H */
