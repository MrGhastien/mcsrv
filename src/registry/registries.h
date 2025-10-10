#ifndef REGISTRIES_H
#define REGISTRIES_H

#include "resource/resource_id.h"

extern const ResourceID REGISTRY_BLOCK_KEY;
extern const ResourceID REGISTRY_BIOME_KEY;
extern const ResourceID REGISTRY_CHAT_TYPE_KEY;
extern const ResourceID REGISTRY_TRIM_PATTERN_KEY;
extern const ResourceID REGISTRY_TRIM_MATERIAL_KEY;
extern const ResourceID REGISTRY_WOLF_VARIANT_KEY;
extern const ResourceID REGISTRY_PAINTING_VARIANT_KEY;
extern const ResourceID REGISTRY_DIMENSION_TYPE_KEY;
extern const ResourceID REGISTRY_DAMAGE_TYPE_KEY;
extern const ResourceID REGISTRY_BANNER_PATTERN_KEY;
extern const ResourceID REGISTRY_ENCHANTMENT_KEY;
extern const ResourceID REGISTRY_JUKEBOX_SONG_KEY;

void register_blocks(void);

#endif /* ! REGISTRIES_H */
