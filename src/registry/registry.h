#ifndef REGISTRY_H
#define REGISTRY_H

#include "definitions.h"
#include "resource/resource_id.h"

void registry_system_init(void);
void registry_system_cleanup(void);

bool registry_create(ResourceID name, u64 stride);

void registry_register(ResourceID registry_name, ResourceID id, void* instance);

#endif /* ! REGISTRY_H */
