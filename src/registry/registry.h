#ifndef REGISTRY_H
#define REGISTRY_H

#include "definitions.h"
#include "resource/resource_id.h"

void registry_system_init(void);
void registry_system_cleanup(void);

bool registry_create(ResourceID name, u64 stride);

void registry_register(ResourceID registry_name, ResourceID id, void* instance);
const void* registry_get(ResourceID registry_name, ResourceID element_id);

i64 registry_create_tag(ResourceID registry_name, ResourceID tag_name);
void registry_tag_add(ResourceID registry_name, i64 tag_idx, ResourceID object_name);
void registry_tag_inherit(ResourceID registry_name, i64 tag_idx, ResourceID inherit_tag_name);

bool registry_is_in_tag(ResourceID registry_name, i64 tag_idx, ResourceID object_name);

#endif /* ! REGISTRY_H */
